#include <QtWidgets>
#include "calibracaodecamaras.h"
#include <iostream>
#include <locale>
#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <vector>
#include <filesystem>
#include <fstream>
#include <locale.h>
#include <regex>
#include <sstream>


using namespace cv;
using namespace std;
namespace fs = std::filesystem;

int boardWidth = 9;
int boardHeight = 6;
Size boardSize(boardWidth, boardHeight);
float squareSize = 3.5; // Chessboard square size
int flaglivecam = 0;
int flagpar = 0;


vector<vector<Point3f>> objectPoints;
vector<vector<Point2f>> imagePoints;
vector<Point3f> objp;
Mat cameraMatrix, distCoeffs;
Mat cameraMatrix2, distCoeffs2;
Mat cameraMatrix3, distCoeffs3;

QImage MatToQImage(const cv::Mat& mat) {
    switch (mat.type()) {
        // 8-bit, 3-channel (color image)
    case CV_8UC3: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped();  // Convert BGR (OpenCV) to RGB (Qt)
    }

                // 8-bit, 1-channel (grayscale image)
    case CV_8UC1: {
        QVector<QRgb> colorTable;
        for (int i = 0; i < 256; i++)
            colorTable.push_back(qRgb(i, i, i));

        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Indexed8);
        image.setColorTable(colorTable);
        return image;
    }

                // 8-bit, 4-channel (e.g., BGRA)
    case CV_8UC4: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return image;
    }

    default:
        qWarning("Unsupported cv::Mat format for QImage conversion");
        return QImage();
    }
}

bool readCalibrationFile(const std::string& filename, Mat& cameraMatrix2, Mat& distCoeffs2) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Failed to open " << filename << endl;
        return false;
    }

    string line;
    string cameraBlock, distBlock;
    bool readingCamera = false, readingDist = false;

    while (getline(file, line)) {
        // Replace commas with dots (decimal formatting)
        replace(line.begin(), line.end(), ',', '.');

        if (line.find("Camera Matrix:") != string::npos) {
            readingCamera = true;
            readingDist = false;
            continue;
        }
        else if (line.find("Distortion Coefficients:") != string::npos) {
            readingCamera = false;
            readingDist = true;
            continue;
        }

        if (readingCamera) {
            cameraBlock += line;
        }
        else if (readingDist) {
            distBlock += line;
        }
    }

    // Helper lambda to extract numbers from string
    auto extractNumbers = [](const string& str) {
        vector<double> numbers;
        regex numRegex(R"([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)");
        sregex_iterator it(str.begin(), str.end(), numRegex);
        sregex_iterator end;
        while (it != end) {
            numbers.push_back(stod(it->str()));
            ++it;
        }
        return numbers;
        };

    vector<double> camVals = extractNumbers(cameraBlock);
    vector<double> distVals = extractNumbers(distBlock);

    if (camVals.size() != 9) {
        cerr << "Error: Invalid camera matrix size (" << camVals.size() << " values found)." << endl;
        return false;
    }

    cameraMatrix2 = Mat(3, 3, CV_64F, camVals.data()).clone();
    distCoeffs2 = Mat(distVals, true).reshape(1, 1); // 1 row
    distCoeffs2.convertTo(distCoeffs2, CV_64F);  // Reshape to a single row

    return true;
}

ClassWizard::ClassWizard(QWidget* parent)
    : QWizard(parent)
{
    
    addPage(new IntroPage);
    addPage(new CameraInfoPage);
    addPage(new ChoicePage);
    addPage(new ImagesPage);
    addPage(new CameraPage);
    addPage(new ConclusionPage);

    setWindowTitle("Camera Calibration");
}

void ClassWizard::accept()
{
    QDialog::accept();
}

IntroPage::IntroPage(QWidget* parent)
    : QWizardPage(parent)
{
    readCalibrationFile("calibration_results.txt", cameraMatrix, distCoeffs);
	readCalibrationFile("calibration_resultslive.txt", cameraMatrix2, distCoeffs2);

    std::locale::global(std::locale("pt_PT.UTF-8"));

    setTitle(u8"Wizard for camera calibration");
	setSubTitle(u8"\nWelcome to the camera calibration wizard. This wizard will guide you through the process of calibrating a camera using either images or a live camera feed.");

    label = new QLabel("\n All data is saved in a database in the program's source folder.\n By default the program uses the last parameters saved on the file calibration_results.txt.");
    label->setWordWrap(true);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(label);
    setLayout(layout);
}

ChoicePage::ChoicePage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Wizard for Camera Calibration");

    choicelabel = new QLabel("Choose between live calibration and image calibration:");
    imagesPageRadioButton = new QRadioButton(tr("Image Calibration"));
    cameraPageRadioButton = new QRadioButton(tr("Live Calibration"));

    QVBoxLayout* choiceLayout = new QVBoxLayout;
    choiceLayout->addWidget(choicelabel);
    choiceLayout->addWidget(imagesPageRadioButton);
    choiceLayout->addWidget(cameraPageRadioButton);

    setLayout(choiceLayout);
}
int ChoicePage::nextId() const
{
    if (imagesPageRadioButton->isChecked()) {
        return ClassWizard::ImagesPageId;
    }
    else if (cameraPageRadioButton->isChecked()) {
        return ClassWizard::CameraPageId;
    }
    return ClassWizard::CameraInfoPageId; // Default to CameraInfoPageId if none selected
}

CameraInfoPage::CameraInfoPage(QWidget* parent)
    : QWizardPage(parent), db(nullptr)
{
    setTitle("Camera Information");
    introtofile = new QLabel("To begin, upload the file with the camera information that will be relevant for the calibration and select the number of rows, columns and square size.\n This program will only consider these parameters valid for the Image Calibration Page, and will use the inputed parameters only once.\n If no information is inputed, the program will continue with default values and create new parameters for calibration:");
	boardheightlbl = new QLabel("Board Height (rows):");
	boardwidthlbl = new QLabel("Board Width (columns):");
	squaresizelbl = new QLabel("Square Size (in cm):");
	boardheightle = new QLineEdit("6");
	boardheightle->setValidator(new QIntValidator(0, 100, this));
	boardwidthle = new QLineEdit("9");
	boardwidthle->setValidator(new QIntValidator(0, 100, this));
	squaresizele = new QLineEdit("1.0");
	squaresizele->setValidator(new QDoubleValidator(0.0, 100.0, 2, this));
    filecont = new QPlainTextEdit();
    uploadButton = new QPushButton("Upload file");

    registerField("filecontent", filecont);

    connect(uploadButton, &QPushButton::clicked, this, &CameraInfoPage::uploadfile);
	connect(boardheightle, &QLineEdit::textChanged, [](const QString& text) {
		boardHeight = text.toInt();
		boardSize = Size(boardWidth, boardHeight);
        cout << "Board Size: " << boardSize.width << "x" << boardSize.height << endl;
        });
	connect(boardwidthle, &QLineEdit::textChanged, [](const QString& text) {
		boardWidth = text.toInt();
		boardSize = Size(boardWidth, boardHeight);
		cout << "Board Size: " << boardSize.width << "x" << boardSize.height << endl;
		});
	connect(squaresizele, &QLineEdit::textChanged, [](const QString& text) {
		squareSize = text.toFloat();
		cout << "Square Size: " << squareSize << " cm" << endl;
		});

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(introtofile, 0, 0, 1, 2);
    layout->addWidget(filecont, 1, 0, 1, 2);
	layout->addWidget(uploadButton, 2, 0, 1, 2);
	layout->addWidget(boardheightlbl, 3, 0);
    layout->addWidget(boardheightle, 3, 1, Qt::AlignRight);
	layout->addWidget(boardwidthlbl, 4, 0);
	layout->addWidget(boardwidthle, 4, 1, Qt::AlignRight);
	layout->addWidget(squaresizelbl, 5, 0);
	layout->addWidget(squaresizele, 5, 1, Qt::AlignRight);
   
    setLayout(layout);
}

void CameraInfoPage::uploadfile()
{
    QString filePath = QFileDialog::getOpenFileName(nullptr, "Open Text File", QString(), "Text Files (*.txt)");
    QString contents;
	string filePathStr = filePath.toStdString();

	if (readCalibrationFile(filePathStr, cameraMatrix, distCoeffs) != true) {
		filecont->setPlainText("Error: Calibration File not in the current format ensure that it reads like in this example:\nCamera Matrix:\n[3317, 651144705852, 0, 2014, 318741125316\n0, 3316, 035857955561, 1674, 495091022121\n0, 0, 1]\nDistortion Coefficients :\n[0, 2132400195360207, -1, 203487041767592, 0, 002274263018278898, -0, 008488002293880892, 2, 165148949643965]\n");
        QMessageBox::warning(this, "Camera Info upload failed", "Please ensure that the file is in the correct format as seen in the textbox");
		return;
	}
    
    readCalibrationFile(filePathStr, cameraMatrix, distCoeffs);
    
    QMessageBox::information(this, "Camera Info uploaded sucessfully!", "File successfully uploaded and coeffs saved:\n" + filePath);

    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            contents = file.readAll();
            file.close();
            QFileInfo filename(filePath);
            filePath = filename.fileName();
            QDate date(QDate::currentDate());
            initializedb();
            uploadtodb(contents, filePath, date.toString("yyyy-MM-dd"));
            closedb();
            filecont->setPlainText(contents);
        }
        else {
            filecont->setPlainText("Error: Unable to open the file.");
        }
    }
    else {
        filecont->setPlainText("File selection canceled.");
    }
    filecont->show();
    flagpar = 1;
    
}

int CameraInfoPage::initializedb()
{
    int result = sqlite3_open("uploadsdatabase.db", &db);
    if (result) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    else {
        const char* sqlCreateTable = "CREATE TABLE IF NOT EXISTS camerainfo ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "date_of_upload TEXT NOT NULL,"
            "file_name TEXT NOT NULL,"
            "contents BLOB NOT NULL);";
        char* errorMessage = 0;
        result = sqlite3_exec(db, sqlCreateTable, 0, 0, &errorMessage);
        if (result != SQLITE_OK) {
            std::cerr << "SQL error: " << errorMessage << std::endl;
            sqlite3_free(errorMessage);
        }
    }
    return result;
}

void CameraInfoPage::uploadtodb(QString contents, QString filename, QString date)
{
    QString sqlInsert = "INSERT INTO camerainfo (date_of_upload, file_name, contents) VALUES ('" +
        date + "', '" + filename + "', '" + contents + "');";

    // Convert the QString to a UTF-8 encoded QByteArray
    QByteArray byteArray = sqlInsert.toUtf8();

    // Execute the SQL query using sqlite3_exec
    int rc = sqlite3_exec(db, byteArray.constData(), nullptr, nullptr, nullptr);

    if (rc != SQLITE_OK) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
        qDebug() << "Failed to execute statement: " << sqlite3_errmsg(db);
    }
}

void CameraInfoPage::closedb()
{
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

ImagesPage::ImagesPage(QWidget* parent)
    : QWizardPage(parent), db(nullptr)
{
    setTitle("Upload imagens for calibration");

    imageLabel = new QLabel;
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFrameStyle(QFrame::Box | QFrame::Sunken);
    imageLabel->setFixedSize(600, 400);

    referenceLabel = new QLabel;
    referenceLabel->setAlignment(Qt::AlignCenter);
    referenceLabel->setFrameStyle(QFrame::Box | QFrame::Sunken);
    referenceLabel->setFixedSize(600, 400);

	progressBar = new QProgressBar;
    int pb;
    progressBar->setRange(0, 100);   // Set min/max
    progressBar->setValue(0);

    uploadButton = new QPushButton("Upload Images");
    downloadButton = new QPushButton("Download Images from DB");
    imageSelector = new QComboBox;

    refimageSelector = new QComboBox;

    calibrationButton = new QPushButton("Calibrate Images");

	info = new QPlainTextEdit;
    info->setReadOnly(1);
    

    connect(uploadButton, &QPushButton::clicked, std::bind(&ImagesPage::uploadImage, this, imageLabel, imageSelector, std::ref(images)));
	connect(downloadButton, &QPushButton::clicked, std::bind(&ImagesPage::downloadImagesToFolder, this, "downloaded_images/"));
    connect(imageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), std::bind(&ImagesPage::displaySelectedImage, this, std::placeholders::_1, imageLabel, std::ref(images)));

    connect(refimageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), std::bind(&ImagesPage::displaySelectedImage, this, std::placeholders::_1, referenceLabel, std::ref(refs)));

    connect(calibrationButton, &QPushButton::clicked, std::bind(&ImagesPage::calibrateImage, this));

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(imageLabel, 0, 0);
    layout->addWidget(uploadButton, 1, 0);
    layout->addWidget(imageSelector, 2, 0);

    layout->addWidget(referenceLabel, 0, 1);
    layout->addWidget(refimageSelector, 1, 1);
	

    layout->addWidget(calibrationButton, 3, 0);
    layout->addWidget(progressBar, 5, 0);
    layout->addWidget(downloadButton, 4, 0);

	layout->addWidget(info, 2, 1, 2, 1);
    setLayout(layout);
}

int ImagesPage::nextId() const
{
    return ClassWizard::ConclusionPageId;
}

void ImagesPage::uploadImage(QLabel* image, QComboBox* selector, QList<QPixmap>& list)
{
    QStringList filePaths = QFileDialog::getOpenFileNames(nullptr, "Open Image Files", QString(), "Image Files (*.png *.jpg *.bmp)");

    fs::remove_all("images/");
    if (!fs::exists("images/")) {
        fs::create_directory("images/");
    }

    if (!filePaths.isEmpty()) {
        foreach(const QString & filePat, filePaths) {

            QPixmap imaget(filePat);
            if (!imaget.isNull()) {
                list.append(imaget);
                selector->addItem(filePat);

                QByteArray imageData = readImageFile(filePat);
                QFileInfo fileInfo(filePat);
                QString imageName = fileInfo.fileName();
                QDate date(QDate::currentDate());

                fs::copy(filePat.toStdString(), "images/" + fileInfo.fileName().toStdString(), fs::copy_options::overwrite_existing);

                initializedb();
                uploadtodb(imageData, imageName, date.toString("yyyy-MM-dd"));
                closedb();
            }
            else {
                image->setText("Error: Unable to load one or more images.");
            }
        }
        if (!list.isEmpty()) {
            displaySelectedImage(0, image, list);
        }
    }
    else {
        image->setText("File selection canceled.");
    }
}

void ImagesPage::uploadUndist(QLabel* image, QComboBox* selector, QList<QPixmap>& list, QString dir)
{
    QDir directory(dir);
    QStringList filePaths = directory.entryList(QDir::Files);

	cout << "Files in directory: " << filePaths.size() << endl;

    if (!filePaths.isEmpty()) {
        foreach(const QString & filePat, filePaths) {
            
            QString fullPath = directory.absoluteFilePath(filePat);
            QPixmap imaget(fullPath);
            if (!imaget.isNull()) {
                list.append(imaget);
                selector->addItem(filePat);

                QByteArray imageData = readImageFile(fullPath);
                QFileInfo fileInfo(fullPath);
                QString imageName = fileInfo.fileName();
                QDate date(QDate::currentDate());

                //fs::copy(filePat.toStdString(), "images/" + fileInfo.fileName().toStdString(), fs::copy_options::overwrite_existing);

                //initializedb();
                //uploadtodb(imageData, imageName/*, date.toString("yyyy-MM-dd")*/);
                //closedb();
				
            }
            else {
                image->setText("Error: Unable to load one or more images.");
            }
        }
        if (!list.isEmpty()) {
            displaySelectedImage(0, image, list);
        }
    }
    else {
        image->setText("File selection canceled.");
    }
}

QByteArray ImagesPage::readImageFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file:" << filePath;
        return QByteArray();
    }
    return file.readAll();
}

void ImagesPage::displaySelectedImage(int index, QLabel* image, QList<QPixmap>& list)
{
    if (index >= 0 && index < list.size()) {
        image->setPixmap(list.at(index).scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

int ImagesPage::initializedb(){
    int result = sqlite3_open("uploadsdatabase.db", &db);
    if (result) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    else {
        const char* sqlCreateTable = "CREATE TABLE IF NOT EXISTS imagesforcal ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "date_of_upload TEXT NOT NULL,"
            "file_name TEXT NOT NULL,"
            "contents BLOB NOT NULL);";
        char* errorMessage = 0;
        result = sqlite3_exec(db, sqlCreateTable, 0, 0, &errorMessage);
        if (result != SQLITE_OK) {
            std::cerr << "SQL error: " << errorMessage << std::endl;
            sqlite3_free(errorMessage);
        }
    }
    return result;
}

void ImagesPage::downloadImagesToFolder(const QString& targetDir)
{
        initializedb();
        // Create target directory if it doesn't exist
        QDir dir(targetDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        const char* sqlSelect = "SELECT file_name, contents FROM imagesforcal;";
        sqlite3_stmt* stmt;

        int rc = sqlite3_prepare_v2(db, sqlSelect, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare SELECT statement: " << sqlite3_errmsg(db) << std::endl;
            return;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            QString fileName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const void* blobData = sqlite3_column_blob(stmt, 1);
            int blobSize = sqlite3_column_bytes(stmt, 1);

            QByteArray imageData(static_cast<const char*>(blobData), blobSize);

            QFile file(dir.filePath(fileName));
            if (file.open(QIODevice::WriteOnly)) {
                file.write(imageData);
                file.close();
            }
            else {
                qDebug() << "Failed to write image:" << fileName;
            }
        }

        sqlite3_finalize(stmt);

        qDebug() << "Images downloaded to:" << dir.absolutePath();
        closedb();
}

void ImagesPage::uploadtodb(QByteArray imageData, QString imageName, QString date)
 {

    const char* sqlInsert = "INSERT INTO imagesforcal (date_of_upload, file_name, contents) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db, sqlInsert, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, date.toUtf8().constData(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, imageName.toUtf8().constData(), -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, imageData.constData(), imageData.size(), SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
}

void ImagesPage::calibrateImage()
{
cv::setBreakOnError(true);
int pb;

    for (int i = 0; i < boardHeight; i++) {
        for (int j = 0; j < boardWidth; j++) {
            objp.push_back(Point3f(j * squareSize, i * squareSize, 0));
        }
    }
	progressBar->setValue(20);

    vector<string> imageFiles;
    for (const auto& entry : fs::directory_iterator("images")) {
        if (entry.path().extension() == ".jpg" || entry.path().extension() == ".png") {
            imageFiles.push_back(entry.path().string());
            cout << "\n";
            cout << entry.path().string()<< endl;
        }
    }
    cout << "\n";
    cout << imageFiles.size()<< endl;

    if (imageFiles.empty()) {
        cerr << "\nError: No images found in 'images' directory." << endl;
    }

    fs::remove_all("chessboard_images/");
    if (!fs::exists("chessboard_images/")) {
        fs::create_directory("chessboard_images/");
    }
    progressBar->setValue(30);

    Mat frame, gray;
    progressBar->setValue(70);
    pb = 70;

    for (const auto& filenam : imageFiles) {
        progressBar->setValue(pb++);
        cout << "\n";
        cout << filenam;
        frame = cv::imread(filenam);
        if (frame.empty()) {
            cerr << "\nWarning: Could not read " << filenam << endl;
            continue;
        }

        cvtColor(frame, gray, COLOR_BGR2GRAY);
        vector<Point2f> corners;
        bool found = findChessboardCorners(gray, boardSize, corners,
            CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_FAST_CHECK | CALIB_CB_NORMALIZE_IMAGE);
        

        if (!found) {
            cerr << "\nWarning: Chessboard not found in " << filenam << endl;
			fs::remove(filenam);  // Remove the image if chessboard is not found
            continue;  // Ignora a imagem sem chessboard
        }
        

        if (found) {
            cornerSubPix(gray, corners, Size(11, 11), Size(-1, -1),
                TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 30, 0.001));

            imagePoints.push_back(corners);
            objectPoints.push_back(objp);

            drawChessboardCorners(frame, boardSize, corners, found);

            string outputFilename = "chessboard_images/drawn_" + fs::path(filenam).filename().string();
            imwrite(outputFilename, frame);
            
            //Para visualizar fora do UI
            //Mat resizedFrame;
            //cv::resize(frame, resizedFrame, Size(frame.cols / 4, frame.rows / 4)); // Resize to half
            //imshow("Chessboard Detection", resizedFrame);
		}
        
    }

    //Update na ImageFiles list
   
    ImagesPage::uploadUndist(referenceLabel, refimageSelector, std::ref(refs), "chessboard_images/");
    //destroyAllWindows();
    
    if (imagePoints.empty()) {
        cerr << "\nError: No valid images for calibration." << endl;
        QMessageBox::warning(this, "Camera Info upload failed", "Error: No valid images for calibration.\n\n Please be certain that you selecte the right amount of columns and rows, and that you uploaded valid images");
		progressBar->setValue(0);
        return;
    }

    //Mat cameraMatrix, distCoeffs;
    vector<Mat> rvecs, tvecs;
    progressBar->setValue(90);
    
    //if (flagpar == 1) {
    cv::calibrateCamera(objectPoints, imagePoints, gray.size(), cameraMatrix, distCoeffs, rvecs, tvecs);

	//}
	//else {
	//	cv::calibrateCamera(objectPoints, imagePoints, gray.size(), cameraMatrix, distCoeffs, rvecs, tvecs);
	//}
    
    ofstream file("calibration_results.txt");
    if (file.is_open()) {
        file << "Camera Matrix:\n" << cameraMatrix << "\n";
        file << "Distortion Coefficients:\n" << distCoeffs << "\n";
        file.close();
        cout << "\nCalibration results saved to 'calibration_results.txt'." << endl;
    }
    else {
        cerr << "\nError: Unable to save calibration data." << endl;
        return;
    }
    

    string coeffs;
    QFile camcoeffs("calibration_results.txt");
    if (camcoeffs.open(QIODevice::ReadOnly | QIODevice::Text)) {
        coeffs = camcoeffs.readAll();
		camcoeffs.close();
        QDate date(QDate::currentDate());
        info->setPlainText(QString::fromStdString(coeffs));
    }
    else {
        info->setPlainText("\nError: Unable to open the file.");
    }

    // Create directory for undistorted images
    if (!fs::exists("undistorted_images")) {
        fs::create_directory("undistorted_images");
    }
    fs::remove_all("undistorted_images/");
    if (!fs::exists("undistorted_images/")) {
        fs::create_directory("undistorted_images/");
    }


	imageFiles.clear();
    for (const auto& entry : fs::directory_iterator("images")) {
        if (entry.path().extension() == ".jpg" || entry.path().extension() == ".png") {
            imageFiles.push_back(entry.path().string());
            cout << "\n";
            cout << entry.path().string() << endl;
        }
    }

    // Save and show undist images
    for (const auto& filenam : imageFiles) {
        Mat original = imread(filenam);
        if (original.empty()) {
            cerr << "\nWarning: Could not read " << filenam << endl;
            continue;
        }

        Mat undistorted;
        undistort(original, undistorted, cameraMatrix, distCoeffs);

        string outputFilename = "undistorted_images/undistorted_" + fs::path(filenam).filename().string();
        imwrite(outputFilename, undistorted);

        //Upload para references  
        
    }
    ImagesPage::uploadUndist(referenceLabel, refimageSelector, std::ref(refs), "undistorted_images/");
    progressBar->setValue(0);
    flagpar = 0;

}

void ImagesPage::closedb(){
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

CameraPage::CameraPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Live Calibration");
    zoom = new QSlider;
    zoom->setOrientation(Qt::Horizontal);
    zoom->setFixedWidth(500);
    zoom->setMaximum(500);
    zoom->setMinimum(0);
    zoom->setSliderPosition(100);
    zoompx = new QLabel;
    framedisplay = new QGraphicsView;
    framedisplay->setFixedHeight(300);
    framedisplay->setFixedWidth(550);
    
    videoWidget = new QVideoWidget;
    videoWidget->setFixedHeight(280);
    videoWidget->setFixedWidth(500);
    combocamaras = new QComboBox;
    paracamara = new QPushButton;
    paracamara->setText("Stop Camera");
    paracamara->setFixedHeight(24);
    paracamara->setFixedWidth(90);
	saveframe = new QPushButton;
	saveframe->setText("Save Frame");
	saveframe->setFixedHeight(24);
	saveframe->setFixedWidth(90);
    mediacapture = new QMediaCaptureSession();
    currentcam = new QCamera();
    qvs = new QVideoSink();
    scene = new QGraphicsScene();
    livecalbutton = new QPushButton();
    livecalbutton->setText("OpenCV Live Calibration");
    undistbutton = new QPushButton();
    undistbutton->setText("Open camera with distortion removal effect");

    connect(combocamaras, &QComboBox::currentIndexChanged, this, &CameraPage::selectcamera);
    connect(paracamara, &QPushButton::clicked, this, &CameraPage::on_paracamara_clicked);
    connect(zoom, &QSlider::valueChanged, this, &CameraPage::on_zoom_valueChanged);
    connect(livecalbutton, &QPushButton::clicked, this, &CameraPage::livecalibration);
    connect(undistbutton, &QPushButton::clicked, this, &CameraPage::undistcamera);
	connect(saveframe, &QPushButton::clicked, this, &CameraPage::on_saveframe_clicked);

    getCameras();
    QGridLayout* layout = new QGridLayout;
    layout->addWidget(videoWidget, 0, 0);
    layout->addWidget(combocamaras, 1, 0);
    layout->addWidget(paracamara, 2, 0);
    layout->addWidget(framedisplay, 0, 1);
    layout->addWidget(zoom, 1, 1);
    layout->addWidget(zoompx, 2, 1);
	layout->addWidget(saveframe, 2, 1, Qt::AlignRight);
    layout->addWidget(livecalbutton, 3, 0);
    layout->addWidget(undistbutton, 3, 1);
    setLayout(layout);
}
void CameraPage::getCameras()
{
    combocamaras->addItem("<None>");
    //coloca na lista "cameras" todos os dispositivos a captar video
    cameras = QMediaDevices::videoInputs();
    //adiciona à comboBox essa lista
    for (const QCameraDevice& camera : cameras)
    {
        combocamaras->addItem(camera.description());
    }
}
void CameraPage::selectcamera(int index)
{
    //contexto: esta função é ativada sempre que o index da ui.combobox é mudado
    //da reset na camera que esta em uso
    if (currentcam->isActive())
    {
        currentcam->stop();
    }
    //a partir da lista de camaras procura a que tem a mesma descrição da selecionada
    for (const QCameraDevice& camera : cameras)
    {
        if (camera.description() == combocamaras->currentText())
        {
            //passa a camara em uso para a que corresponde ao indice da comboBox
            currentcam->setCameraDevice(camera);
            //mediacapturesession para poder transmitir o video para o widget da classe QVideoWidget
            mediacapture->setCamera(currentcam);
            mediacapture->setVideoOutput(videoWidget);
            //cria um video sink associado a esta mediacapture para extrair frames (ver a função on_paracamara_clicked) 
            qvs = mediacapture->videoSink();
            currentcam->start();
            break;
        }
    }
}
void CameraPage::on_zoom_valueChanged()
{
    //troca o texto que representa a posicao do slider "zoom"
    zoompx->setText("Zoom: " + QString::number(zoom->value()) + "%");
    // extrai o valor do slider para uma variavel e calcula a escala
    qreal scaleFactor = static_cast<qreal>(zoom->value()) / 100.0;
    
    framedisplay->resetTransform();
    framedisplay->scale(scaleFactor, scaleFactor);
}
void CameraPage::on_paracamara_clicked()
{
    //contexto: esta função funciona quando o botao "para camara" é clicado, ui.framedisplay é um objeto do tipo QGraphicsView
    if (!qvs)
        return;
    // usa o video sink associado em selectcamera para tirar um frame
    QVideoFrame frame = qvs->videoFrame();
    if (!frame.isValid())
        return;
    // converte o frame em imagem e passa de imagem para pixmap
    QImage img = frame.toImage();
    img.save("imageslive/frame.png", "png"); // Save the frame as an image file

    Mat original = imread("imageslive/frame.png");

    Mat undistorted;
    undistort(original, undistorted, cameraMatrix, distCoeffs);

	img=MatToQImage(undistorted); // Save the undistorted frame

    QPixmap pixmap = QPixmap::fromImage(img);
    // usa uma QGraphicsScene para colocar no graphicsview
    scene->clear();
    scene->addPixmap(pixmap);
    // muda a scene e mostra no graphicsview
    framedisplay->setScene(scene);
    framedisplay->show();
}
void CameraPage::on_saveframe_clicked()
{
    if (!framedisplay || !scene)
        return;

    // Get the zoom scale from the slider
    qreal scaleFactor = static_cast<qreal>(zoom->value()) / 100.0;

	// Guarda a parte visivel do framedisplay com o zoom aplicado
    QSize viewSize = framedisplay->viewport()->size();
    QSize scaledSize = viewSize * scaleFactor;

	// Renderiza o que está no framedisplay para um QPixmap
    QPixmap pixmap(scaledSize);
    QPainter painter(&pixmap);
    framedisplay->render(&painter);
    painter.end();

    // Save the final image
    QString baseName = "imageslive/saved_frame.jpg";
    int counter = 0;
    QString filename;
    do {
        filename = baseName + QString::number(counter++) + ".jpg";
    } while (QFile::exists(filename));

    if (pixmap.save(filename)) {
        QMessageBox::information(this, "Frame Saved", "Image saved successfully to:\n" + filename);
    }
    else {
        QMessageBox::warning(this, "Save Failed", "Failed to save the image.");
    }
}
float CameraPage::livecalibration() {

	flaglivecam = 1;
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Error: Unable to open the camera." << endl;
    }

    vector<Point3f> objp;
    for (int i = 0; i < boardHeight; i++) {
        for (int j = 0; j < boardWidth; j++) {
            objp.push_back(Point3f(j * squareSize, i * squareSize, 0));
        }
    }

    Mat frame, gray;
    int numFrames = 0;
    while (numFrames < 15) { // Capture 15 valid images
        cap >> frame;
        if (frame.empty()) {
            cerr << "Error: Frame capture failed." << endl;
            break;
        }

        cvtColor(frame, gray, COLOR_BGR2GRAY);
        vector<Point2f> corners;
        bool found = findChessboardCorners(gray, boardSize, corners,
            CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_FAST_CHECK | CALIB_CB_NORMALIZE_IMAGE);

        if (found) {
            cornerSubPix(gray, corners, Size(11, 11), Size(-1, -1),
                TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 30, 0.001));

            drawChessboardCorners(frame, boardSize, corners, found);
            imagePoints.push_back(corners);
            objectPoints.push_back(objp);

            cout << "Captured frame " << numFrames + 1 << endl;
            numFrames++;
        }

        imshow("Camera Calibration", frame);
        char key = (char)waitKey(1000);
        if (key == 27) { // Press 'ESC' to exit
            break;
        }
    }

    cap.release();
    cv::waitKey(1);
    destroyAllWindows();

    // Camera calibration
    vector<Mat> rvecs, tvecs;

    cv::calibrateCamera(objectPoints, imagePoints, gray.size(), cameraMatrix2, distCoeffs2, rvecs, tvecs);

    cout << "Camera Matrix: \n" << cameraMatrix2 << endl;
    cout << "Distortion Coefficients: \n" << distCoeffs2 << endl;

    // Save calibration data
    ofstream file("calibration_resultslive.txt");
    if (file.is_open()) {
        file << "Camera Matrix:\n" << cameraMatrix2 << "\n";
        file << "Distortion Coefficients:\n" << distCoeffs2 << "\n";
        file.close();
    }
  
}
void CameraPage::undistcamera()
{
	/*if (flaglivecam == 0) {
        QMessageBox::warning(
            this,                                   // Parent widget
            "Warning",                              // Window title
            "Please use live calibration before opening the undistorted camera."  // Message text
        );
        return;
	}*/
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        QMessageBox::warning(
            this,                                   // Parent widget
            "Warning",                              // Window title
            "Error: Unable to open the camera for distortion display."  // Message text
        );
		return;
    }

    while (true) {
        Mat frame;
        cap >> frame;
        if (frame.empty()) {
            cerr << "Error: Frame capture failed." << endl;
            break;
        }

        // Apply distortion effect
        Mat distortedFrame;
        Mat map1, map2;
        initUndistortRectifyMap(cameraMatrix2, distCoeffs2, Mat(), cameraMatrix2, frame.size(), CV_32FC1, map1, map2);
        remap(frame, distortedFrame, map1, map2, INTER_LINEAR, BORDER_CONSTANT);

        imshow("Undistorted Camera View", distortedFrame);

        char key = (char)waitKey(30);
        if (key == 27) { // Press 'ESC' to exit
            break;
        }
    }

    cap.release();
    cv::waitKey(1);
    destroyAllWindows();

}

ConclusionPage::ConclusionPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(tr("Conclusion"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/watermark2.png"));

    label = new QLabel;
    label->setWordWrap(true);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(label);
    setLayout(layout);
}

void ConclusionPage::initializePage()
{
    QString finishText = wizard()->buttonText(QWizard::FinishButton);
    finishText.remove('&');
    label->setText(tr("Click %1 to generate the class skeleton.")
        .arg(finishText));
}


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

using namespace cv;
using namespace std;
namespace fs = std::filesystem;

const int boardWidth = 9;
const int boardHeight = 6;
const Size boardSize(boardWidth, boardHeight);
float squareSize = 1.0; // Chessboard square size

vector<vector<Point3f>> objectPoints;
vector<vector<Point2f>> imagePoints;
vector<Point3f> objp;
Mat cameraMatrix, distCoeffs;


ClassWizard::ClassWizard(QWidget* parent)
    : QWizard(parent)
{
    addPage(new IntroPage);
    addPage(new CameraInfoPage);
    addPage(new ChoicePage);
    addPage(new ImagesPage);
    addPage(new CameraPage);
    addPage(new ConclusionPage);

    setWindowTitle("Calibração de camaras");
}

void ClassWizard::accept()
{
    QDialog::accept();
}

IntroPage::IntroPage(QWidget* parent)
    : QWizardPage(parent)
{
    std::locale::global(std::locale("pt_PT.UTF-8"));

    setTitle(u8"Wizard para calibração de cameras");

    label = new QLabel(" Este Wizard tem o intuito de ajudar o utilizador a calibrar uma camera \n\n Com este programa o utilizador tem duas opcoes para efetuar a calibracao (Por Imagens ou Camara Livre), todos os dados sao guardados numa base de dados na pasta fonte do programa.");
    label->setWordWrap(true);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(label);
    setLayout(layout);
}

ChoicePage::ChoicePage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Wizard para calibração de cameras");

    choicelabel = new QLabel("Escolha entre calibração de camara livre ou por imagens:");
    imagesPageRadioButton = new QRadioButton(tr("Por Imagens"));
    cameraPageRadioButton = new QRadioButton(tr("Câmera Livre"));

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
    setTitle("Informacao da camara");
    introtofile = new QLabel("Para começar, carregue o ficheiro com as informacoes da camara que serao relevantes para a calibracao:");
    filecont = new QPlainTextEdit();
    uploadButton = new QPushButton("Carregar ficheiro");

    registerField("filecontent", filecont);

    connect(uploadButton, &QPushButton::clicked, this, &CameraInfoPage::uploadfile);

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(introtofile, 0, 0);
    layout->addWidget(filecont, 1, 0);
    layout->addWidget(uploadButton, 2, 0);
   
    setLayout(layout);
}

void CameraInfoPage::uploadfile()
{
    /*QString filePath = QFileDialog::getOpenFileName(nullptr, "Open Text File", QString(), "Text Files (*.txt)");
    QString contents;
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
    filecont->show();*/
    
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
    setTitle("Carregar imagens para calibração");

    imageLabel = new QLabel;
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFrameStyle(QFrame::Box | QFrame::Sunken);
    imageLabel->setFixedSize(600, 400);

    referenceLabel = new QLabel;
    referenceLabel->setAlignment(Qt::AlignCenter);
    referenceLabel->setFrameStyle(QFrame::Box | QFrame::Sunken);
    referenceLabel->setFixedSize(600, 400);

    uploadButton = new QPushButton("Upload Images");
    imageSelector = new QComboBox;

    refimageSelector = new QComboBox;

    calibrationButton = new QPushButton("Calibrate Images");

    connect(uploadButton, &QPushButton::clicked, std::bind(&ImagesPage::uploadImage, this, imageLabel, imageSelector, std::ref(images)));
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
                uploadtodb(imageData, imageName/*, date.toString("yyyy-MM-dd")*/);
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

void ImagesPage::uploadUndist(QLabel* image, QComboBox* selector, QList<QPixmap>& list)
{
    QDir directory("undistorted_images/");
    QStringList filePaths = directory.entryList(QDir::Files);

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

void ImagesPage::uploadtodb(QByteArray imageData, QString imageName/*, QString date*/)
 {

    //QString sqlInsert = "INSERT INTO imagesforcal (date_of_upload, file_name, contents) VALUES ('" +
    //    date + "', '" + imageName + "', '" + imageData + "');";

    //// Convert the QString to a UTF-8 encoded QByteArray
    //QByteArray byteArray = sqlInsert.toUtf8();

    //// Execute the SQL query using sqlite3_exec
    //int rc = sqlite3_exec(db, byteArray.constData(), nullptr, nullptr, nullptr);

    //if (rc != SQLITE_OK) {
    //    std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
    //    qDebug() << "Failed to execute statement: " << sqlite3_errmsg(db);

    //}

    const char* sqlInsert = "INSERT INTO imagesforcal (image_name, image_data) VALUES (?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db, sqlInsert, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, imageName.toUtf8().constData(), -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, imageData.constData(), imageData.size(), SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
}

void ImagesPage::calibrateImage()
{
cv::setBreakOnError(true);

    for (int i = 0; i < boardHeight; i++) {
        for (int j = 0; j < boardWidth; j++) {
            objp.push_back(Point3f(j * squareSize, i * squareSize, 0));
        }
    }

    vector<string> imageFiles;
    for (const auto& entry : fs::directory_iterator("images")) {
        if (entry.path().extension() == ".jpg" || entry.path().extension() == ".png") {
            imageFiles.push_back(entry.path().string());
            cout << entry.path().string()<< endl;
        }
    }
    cout << imageFiles.size()<< endl;

    if (imageFiles.empty()) {
        cerr << "Error: No images found in 'images' directory." << endl;
    }


    Mat frame, gray;
    for (const auto& filenam : imageFiles) {
        cout << filenam;
        frame = cv::imread(filenam);
        if (frame.empty()) {
            cerr << "Warning: Could not read " << filenam << endl;
            continue;
        }

        cvtColor(frame, gray, COLOR_BGR2GRAY);
        vector<Point2f> corners;
        bool found = findChessboardCorners(gray, boardSize, corners,
            CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_FAST_CHECK | CALIB_CB_NORMALIZE_IMAGE);

        if (found) {
            cornerSubPix(gray, corners, Size(11, 11), Size(-1, -1),
                TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 30, 0.001));

            imagePoints.push_back(corners);
            objectPoints.push_back(objp);

            drawChessboardCorners(frame, boardSize, corners, found);

            //string outputFilename = "chessboard_images/drawn_" + fs::path(filenam).filename().string();
            //imwrite(outputFilename, frame);

            
            //ImagesPage::uploadUndist(imageLabel, imageSelector, std::ref(images), "chessboard_images/");
            //Para visualizar fora do UI
            //Mat resizedFrame;
            //cv::resize(frame, resizedFrame, Size(frame.cols / 4, frame.rows / 4)); // Resize to half
            //imshow("Chessboard Detection", resizedFrame);
        }
    }
    //destroyAllWindows();
    
    if (imagePoints.empty()) {
        cerr << "Error: No valid images for calibration." << endl;
    }

    Mat cameraMatrix, distCoeffs;
    vector<Mat> rvecs, tvecs;
    calibrateCamera(objectPoints, imagePoints, gray.size(), cameraMatrix, distCoeffs, rvecs, tvecs);

    ofstream file("calibration_results.txt");
    if (file.is_open()) {
        file << "Camera Matrix:\n" << cameraMatrix << "\n";
        file << "Distortion Coefficients:\n" << distCoeffs << "\n";
        file.close();
        //cout << "Calibration results saved to 'calibration_results.txt'." << endl;
    }
    else {
        cerr << "Error: Unable to save calibration data." << endl;
    }

    // Create directory for undistorted images
    if (!fs::exists("undistorted_images")) {
        fs::create_directory("undistorted_images");
    }

    // Save and show undist images
    for (const auto& filename : imageFiles) {
        Mat original = imread(filename);
        if (original.empty()) {
            cerr << "Warning: Could not read " << filename << endl;
            continue;
        }

        Mat undistorted;
        undistort(original, undistorted, cameraMatrix, distCoeffs);

        string outputFilename = "undistorted_images/undistorted_" + fs::path(filename).filename().string();
        imwrite(outputFilename, undistorted);

        //Upload para references
        ImagesPage::uploadUndist(referenceLabel, refimageSelector, std::ref(refs));
       //Para visualizar fora do UI
       // cv::resize(undistorted, resizedUndistorted, Size(undistorted.cols / 4, undistorted.rows / 4));
       // imshow("Undistorted Image", resizedUndistorted);
    }
    //destroyAllWindows();
    // Chessboard dimensions (number of inner corners per chessboard row and column)
    

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
    setTitle("Calibracao em Camara Livre");
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
    paracamara->setText("Parar Camara");
    paracamara->setFixedHeight(24);
    paracamara->setFixedWidth(90);
    mediacapture = new QMediaCaptureSession();
    currentcam = new QCamera();
    qvs = new QVideoSink();
    scene = new QGraphicsScene();
    livecalbutton = new QPushButton();
    livecalbutton->setText("Calibracao com OpenCV Live");
    undistbutton = new QPushButton();
    undistbutton->setText("Abrir camera com Distorcao ativada");

    connect(combocamaras, &QComboBox::currentIndexChanged, this, &CameraPage::selectcamera);
    connect(paracamara, &QPushButton::clicked, this, &CameraPage::on_paracamara_clicked);
    connect(zoom, &QSlider::valueChanged, this, &CameraPage::on_zoom_valueChanged);
    connect(livecalbutton, &QPushButton::clicked, this, &CameraPage::livecalibration);
    connect(undistbutton, &QPushButton::clicked, this, &CameraPage::undistcamera);
    getCameras();
    QGridLayout* layout = new QGridLayout;
    layout->addWidget(videoWidget, 0, 0);
    layout->addWidget(combocamaras, 1, 0);
    layout->addWidget(paracamara, 2, 0);
    layout->addWidget(framedisplay, 0, 1);
    layout->addWidget(zoom, 1, 1);
    layout->addWidget(zoompx, 2, 1);
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
    qDebug() << scaleFactor << "\n";
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
    QPixmap pixmap = QPixmap::fromImage(img);
    // usa uma QGraphicsScene para colocar no graphicsview
    scene->clear();
    scene->addPixmap(pixmap);
    // muda a scene e mostra no graphicsview
    framedisplay->setScene(scene);
    framedisplay->show();
}
float CameraPage::livecalibration() {

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
    calibrateCamera(objectPoints, imagePoints, gray.size(), cameraMatrix, distCoeffs, rvecs, tvecs);

    cout << "Camera Matrix: \n" << cameraMatrix << endl;
    cout << "Distortion Coefficients: \n" << distCoeffs << endl;

    // Save calibration data
    ofstream file("calibration_results.txt");
    if (file.is_open()) {
        file << "Camera Matrix:\n" << cameraMatrix << "\n";
        file << "Distortion Coefficients:\n" << distCoeffs << "\n";
        file.close();
    }
  
}
void CameraPage::undistcamera()
{
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Error: Unable to open the camera for distortion display." << endl;
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
        initUndistortRectifyMap(cameraMatrix, distCoeffs, Mat(), cameraMatrix, frame.size(), CV_32FC1, map1, map2);
        remap(frame, distortedFrame, map1, map2, INTER_LINEAR, BORDER_CONSTANT);

        imshow("Distorted Camera View", distortedFrame);

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


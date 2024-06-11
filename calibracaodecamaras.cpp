#include <QtWidgets>
#include "calibracaodecamaras.h"
#include <iostream>

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
    setTitle("Wizard para calibração de cameras");

    label = new QLabel("Este Wizard tem o intuito de ajudar o utilizador a calibrar uma camera");
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

    groupBox = new QGroupBox();

    registerField("filecontent", filecont);

    connect(uploadButton, &QPushButton::clicked, this, &CameraInfoPage::uploadfile);

    QVBoxLayout* groupBoxLayout = new QVBoxLayout;
    groupBoxLayout->addWidget(filecont);
    groupBoxLayout->addWidget(uploadButton);
    groupBox->setLayout(groupBoxLayout);

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(introtofile, 0, 0);
    layout->addWidget(filecont, 1, 0);
    layout->addWidget(uploadButton, 2, 0);
    layout->addWidget(groupBox, 3, 0, 1, 2);
    setLayout(layout);
}

void CameraInfoPage::uploadfile()
{
    QString filePath = QFileDialog::getOpenFileName(nullptr, "Open Text File", QString(), "Text Files (*.txt)");
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
    filecont->show();
    
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
    : QWizardPage(parent)
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

    refuploadButton = new QPushButton("Upload References");
    refimageSelector = new QComboBox;

    connect(uploadButton, &QPushButton::clicked, std::bind(&ImagesPage::uploadImage, this, imageLabel, imageSelector, std::ref(images)));
    connect(imageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), std::bind(&ImagesPage::displaySelectedImage, this, std::placeholders::_1, imageLabel, std::ref(images)));

    connect(refuploadButton, &QPushButton::clicked, std::bind(&ImagesPage::uploadImage, this, referenceLabel, refimageSelector, std::ref(refs)));
    connect(refimageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), std::bind(&ImagesPage::displaySelectedImage, this, std::placeholders::_1, referenceLabel, std::ref(refs)));

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(imageLabel, 0, 0);
    layout->addWidget(uploadButton, 1, 0);
    layout->addWidget(imageSelector, 2, 0);

    layout->addWidget(referenceLabel, 0, 1);
    layout->addWidget(refuploadButton, 1, 1);
    layout->addWidget(refimageSelector, 2, 1);
    setLayout(layout);
}
int ImagesPage::nextId() const
{
    return ClassWizard::ConclusionPageId;
}

void ImagesPage::uploadImage(QLabel* image, QComboBox* selector, QList<QPixmap>& list)
{
    QStringList filePaths = QFileDialog::getOpenFileNames(nullptr, "Open Image Files", QString(), "Image Files (*.png *.jpg *.bmp)");

    if (!filePaths.isEmpty()) {
        foreach(const QString & filePath, filePaths) {
            QPixmap imaget(filePath);
            if (!imaget.isNull()) {
                list.append(imaget);
                selector->addItem(filePath);
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

void ImagesPage::displaySelectedImage(int index, QLabel* image, QList<QPixmap>& list)
{
    if (index >= 0 && index < list.size()) {
        image->setPixmap(list.at(index).scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

CameraPage::CameraPage(QWidget* parent)
    : QWizardPage(parent), currentcam(nullptr), mediacapture(new QMediaCaptureSession()), qvs(nullptr), scene(new QGraphicsScene())
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

    videoWidget = new QVideoWidget();
    videoWidget->setFixedHeight(280);
    videoWidget->setFixedWidth(500);

    combocamaras = new QComboBox;
    paracamara = new QPushButton;
    paracamara->setText("Parar Camara");
    paracamara->setFixedHeight(24);
    paracamara->setFixedWidth(90);

    connect(combocamaras, &QComboBox::currentIndexChanged, this, &CameraPage::selectcamera);
    connect(paracamara, &QPushButton::clicked, this, &CameraPage::on_paracamara_clicked);
    connect(zoom, &QSlider::valueChanged, this, &CameraPage::on_zoom_valueChanged);

    getCameras();

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(videoWidget, 0, 0);
    layout->addWidget(combocamaras, 1, 0);
    layout->addWidget(paracamara, 2, 0);

    layout->addWidget(framedisplay, 0, 1);
    layout->addWidget(zoom, 1, 1);
    layout->addWidget(zoompx, 2, 1);
    setLayout(layout);
}

void CameraPage::getCameras()
{
    combocamaras->addItem("<None>");
    cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice& camera : cameras)
    {
        combocamaras->addItem(camera.description());
    }
}

void CameraPage::selectcamera(int index)
{
    if (currentcam && currentcam->isActive())
    {
        currentcam->stop();
    }
    if (index > 0 && index <= cameras.size()) {
        const QCameraDevice& camera = cameras.at(index - 1);
        currentcam = new QCamera(camera);
        mediacapture->setCamera(currentcam);
        mediacapture->setVideoOutput(videoWidget);
        qvs = new QVideoSink();
        mediacapture->setVideoSink(qvs);
        currentcam->start();
    }
}

void CameraPage::on_zoom_valueChanged()
{
    zoompx->setText("Zoom: " + QString::number(zoom->value()) + "%");
    qreal scaleFactor = static_cast<qreal>(zoom->value()) / 100.0;
    framedisplay->resetTransform();
    framedisplay->scale(scaleFactor, scaleFactor);
}

void CameraPage::on_paracamara_clicked()
{
    if (!qvs)
        return;
    QVideoFrame frame = qvs->videoFrame();
    if (!frame.isValid())
        return;
    QImage img = frame.toImage();
    QPixmap pixmap = QPixmap::fromImage(img);
    scene->clear();
    scene->addPixmap(pixmap);
    framedisplay->setScene(scene);
    framedisplay->show();
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

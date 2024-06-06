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
        return ClassWizard::ImagesPageId;  // Replace with the actual ID of ImagesPage
    }
    else if (cameraPageRadioButton->isChecked()) {
        return ClassWizard::CameraPageId;  // Replace with the actual ID of CameraPage
    }
}

CameraInfoPage::CameraInfoPage(QWidget* parent)
    : QWizardPage(parent)
{
    //! [8]
    setTitle("Informacao da camara");
    introtofile = new QLabel("Para começar, carregue o ficheiro com as informacoes da camara que serao relevantes para a calibracao:");
    filecont = new QPlainTextEdit();
    uploadButton = new QPushButton("Carregar ficheiro");
    //uploadButton->setText("Carregar ficheiro");

    groupBox = new QGroupBox();
 
    //guarda o valor do file cont para ser usado entre wizard pages
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

void CameraInfoPage::uploadfile() {

    QString filePath = QFileDialog::getOpenFileName(nullptr, "Open Text File", QString(), "Text Files (*.txt)");
    QString contents;
    // Check if a file was selected
    if (!filePath.isEmpty()) {
        // Read the contents of the selected text file
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // Read all text from the file
            contents = file.readAll();
            file.close();

            // Set the text file contents to the text edit widget
            filecont->setPlainText(contents);
        }
        else {
            // Error handling: Failed to open the file
            filecont->setPlainText("Error: Unable to open the file.");
        }
    }
    else {
        // User canceled file selection
        filecont->setPlainText("File selection canceled.");
    }

    // Show the text edit widget
    filecont->show();

    QFileInfo filename(filePath);
    filePath= filename.fileName();

    QDate date(QDate::currentDate());

    initializedb();
    uploadtodb(contents, filePath, date.toString("yyyy-MM-dd"));
    
    
}

int CameraInfoPage::initializedb()
{
    sqlite3* db;
    char* errorMessage = 0;
    int result;

    // Open (or create) a database named "uploadsdatabase.db"
    result = sqlite3_open("uploadsdatabase.db", &db);
    if (result) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    else {
        std::cout << "Opened database successfully" << std::endl;
    }

    // Create a table named "camerainfo" if it doesn't already exist
    const char* sqlCreateTable = "CREATE TABLE IF NOT EXISTS camerainfo ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date_of_upload TEXT NOT NULL,"
        "file_name TEXT NOT NULL,"
        "contents BLOB NOT NULL);";

    result = sqlite3_exec(db, sqlCreateTable, 0, 0, &errorMessage);
    if (result != SQLITE_OK) {
        std::cerr << "SQL error: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
    }
    else {
        std::cout << "Table created successfully" << std::endl;
    }

}

void CameraInfoPage::uploadtodb(QString contents, QString filename, QString date){

}

void CameraInfoPage::closedb()
{

}

ImagesPage::ImagesPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Carregar imagens para calibração");

    imageLabel = new QLabel;
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFrameStyle(QFrame::Box | QFrame::Sunken);
    imageLabel->setFixedSize(600, 400);  // Set the desired fixed size for the image display

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
    layout->addWidget(imageLabel,0,0);
    layout->addWidget(uploadButton,1,0);
    layout->addWidget(imageSelector,2,0);

    layout->addWidget(referenceLabel, 0, 1);
    layout->addWidget(refuploadButton,1,1);
    layout->addWidget(refimageSelector,2,1);
    setLayout(layout);
}
int ImagesPage::nextId() const
{
    return ClassWizard::ConclusionPageId;
}

void ImagesPage::uploadImage(QLabel* image,QComboBox* selector, QList<QPixmap>& list)
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
                // Error handling: Unable to load the image
                image->setText("Error: Unable to load one or more images.");
            }
        }
        // Display the first image by default
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

    videoWidget = new QVideoWidget();
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

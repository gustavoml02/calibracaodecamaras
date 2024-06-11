#ifndef CALIBRACAODECAMARAS_H
#define CALIBRACAODECAMARAS_H

#include <QWizard>
#include <QCamera>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QImage>
#include <QGraphicsScene>
#include <QPixmap>
#include <QSlider>
#include <QVideoWidget>
#include "sqlite3.h"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QGroupBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;
class QFileInfo;
class QDate;
QT_END_NAMESPACE

class ClassWizard : public QWizard
{
    Q_OBJECT

public:
    ClassWizard(QWidget* parent = nullptr);

    static const int IntroPageId = 0;
    static const int CameraInfoPageId = 1;
    static const int ChoicePageId = 2;
    static const int ImagesPageId = 3;
    static const int CameraPageId = 4;
    static const int ConclusionPageId = 5;

    void accept() override;
};

class IntroPage : public QWizardPage
{
    Q_OBJECT

public:
    IntroPage(QWidget* parent = nullptr);

private:
    QLabel* label;
};

class ChoicePage : public QWizardPage
{
    Q_OBJECT

public:
    ChoicePage(QWidget* parent = nullptr);
    int nextId() const override;

private:
    QLabel* choicelabel;
    QVBoxLayout* choiceLayout;
    QRadioButton* imagesPageRadioButton;
    QRadioButton* cameraPageRadioButton;
};

class CameraInfoPage : public QWizardPage
{
    Q_OBJECT

public:
    CameraInfoPage(QWidget* parent = nullptr);

    void uploadfile();
    int initializedb();
    void closedb();
    void uploadtodb(QString contents, QString filename, QString date);

private:
    QGroupBox* groupBox;
    QLabel* introtofile;
    QPlainTextEdit* filecont;
    QPushButton* uploadButton;
    sqlite3* db;
};

class ImagesPage : public QWizardPage
{
    Q_OBJECT

public:
    ImagesPage(QWidget* parent = nullptr);
    int nextId() const override;

private slots:
    void uploadImage(QLabel* image, QComboBox* selector, QList<QPixmap>& list);
    void displaySelectedImage(int index, QLabel* image, QList<QPixmap>& list);
    //void initializedb();
   // void closedb();
    //void uploadtodb(QImage* image, int i);

private:
    QLabel* imageLabel;
    QLabel* referenceLabel;
    QPushButton* uploadButton;
    QPushButton* refuploadButton;
    QComboBox* imageSelector;
    QComboBox* refimageSelector;
    QList<QPixmap> images;
    QList<QPixmap> refs;
};

class CameraPage : public QWizardPage
{
    Q_OBJECT

public:
    CameraPage(QWidget* parent = nullptr);
    QList<QCameraDevice> cameras;
    QCamera* currentcam;
    QMediaCaptureSession* mediacapture;
    QVideoSink* qvs;
    QGraphicsScene* scene;
    QSlider* zoom;
    QPushButton* paracamara;
    QPushButton* gridbutton;
    QComboBox* combocamaras;
    QVideoWidget* videoWidget;
    QLabel* zoompx;
    QLabel* zoomlab;
    QGraphicsView* framedisplay;

private slots:
    void getCameras();
    void selectcamera(int index);
    void on_zoom_valueChanged();
    void on_paracamara_clicked();

private:
    QLabel* label;
};

class ConclusionPage : public QWizardPage
{
    Q_OBJECT

public:
    ConclusionPage(QWidget* parent = nullptr);

protected:
    void initializePage() override;

private:
    QLabel* label;
};

#endif

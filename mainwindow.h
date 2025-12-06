#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLineEdit>
#include <QtGui/QImage>
#include "jpegloader.h"
#include "jpegsaver.h"
#include "imagehandler.h"
#include "jpegclient.h"

class MainWindow : public QMainWindow, public ImageLoadObserver
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Реализация ImageLoadObserver
    void onImageLoaded(const QImage& image) override;
    void onLoadError(const QString& error) override;

private slots:
    void onLoadButtonClicked();
    void onSaveButtonClicked();
    void onNextScanButtonClicked();
    void onQualityChanged(int value);
    void onNetworkLoadButtonClicked();
    void onNetworkImageReceived(const QImage& image);
    void onNetworkError(const QString& error);
    void onUploadButtonClicked();
    void onUploadFinished(bool success, const QString& message);

private:
    QLabel* imageLabel;
    QPushButton* loadButton;
    QPushButton* saveButton;
    QPushButton* nextScanButton;
    QPushButton* networkLoadButton;
    QPushButton* uploadButton;
    QLineEdit* ipEdit;
    QLineEdit* portEdit;
    QCheckBox* progressiveCheckBox;
    QComboBox* dctComboBox;
    QSlider* qualitySlider;
    QSpinBox* qualitySpinBox;

    QImage currentImage;
    ImageHandler* imageHandler;
    LoadImageCommand* loadCommand;

    class JPEGClient* networkClient;

    void setupUI();
    void updateImageDisplay(const QImage& image);
    void updateNextScanButton();
};

#endif // MAINWINDOW_H


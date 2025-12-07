#include "mainwindow.h"
#include <QtWidgets/QApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , imageHandler(nullptr)
    , loadCommand(nullptr)
    , networkClient(nullptr)
    , networkSslClient(nullptr)
    , serverManager(nullptr)
{
    // Initialize objects BEFORE setupUI() to avoid nullptr in connect()
    imageHandler = ImageHandler::createHandler(ImageHandler::Progressive);
    serverManager = new ServerManager(this);
    networkClient = new JPEGClient(this);
    networkSslClient = new JPEGSslClient(this);
    
    setupUI();
}

MainWindow::~MainWindow()
{
    delete imageHandler;
    delete loadCommand;
    if (serverManager) {
        serverManager->stopServer();
    }
}

void MainWindow::setupUI()
{
    setWindowTitle("JPEG Viewer with Progressive Loading");
    setMinimumSize(1200, 900);
    resize(1200, 900);

    statusBar()->showMessage("Ready");
    
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);


    QHBoxLayout* buttonLayout = new QHBoxLayout();
    loadButton = new QPushButton("Load JPEG", this);
    saveButton = new QPushButton("Save JPEG", this);
    nextScanButton = new QPushButton(">", this);
    nextScanButton->setEnabled(false);
    nextScanButton->setMaximumWidth(50);

    // Network controls
    buttonLayout->addWidget(new QLabel("Client Mode:", this));
    clientModeComboBox = new QComboBox(this);
    clientModeComboBox->addItem("Normal", 0);
    clientModeComboBox->addItem("Secure (SSL)", 1);
    clientModeComboBox->setFixedWidth(120);
    buttonLayout->addWidget(clientModeComboBox);
    
    networkLoadButton = new QPushButton("Get from Server", this);
    ipEdit = new QLineEdit(this);
    ipEdit->setPlaceholderText("IP address");
    ipEdit->setFixedWidth(120);
    portEdit = new QLineEdit(this);
    portEdit->setPlaceholderText("Port");
    portEdit->setFixedWidth(60);
    portEdit->setText("12345");

    buttonLayout->addWidget(loadButton);
    buttonLayout->addWidget(saveButton);
    // Upload button
    uploadButton = new QPushButton("Upload to Server", this);
    uploadButton->setFixedWidth(140);
    buttonLayout->addWidget(nextScanButton);
    buttonLayout->addSpacing(20);
    buttonLayout->addWidget(ipEdit);
    buttonLayout->addWidget(portEdit);
    buttonLayout->addWidget(networkLoadButton);
    buttonLayout->addWidget(uploadButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    imageLabel = new QLabel(this);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setStyleSheet("QLabel { border: 2px solid gray; background-color: #2b2b2b; }");
    imageLabel->setMinimumSize(1000, 700);
    imageLabel->setText("No image loaded");
    imageLabel->setScaledContents(false);
    mainLayout->addWidget(imageLabel);

    QHBoxLayout* saveOptionsLayout = new QHBoxLayout();
    
    progressiveCheckBox = new QCheckBox("Progressive", this);
    saveOptionsLayout->addWidget(progressiveCheckBox);
    
    saveOptionsLayout->addWidget(new QLabel("DCT Method:", this));
    dctComboBox = new QComboBox(this);
    dctComboBox->addItem("Integer", 0);
    dctComboBox->addItem("Fast", 1);
    dctComboBox->addItem("Float", 2);
    saveOptionsLayout->addWidget(dctComboBox);
    
    saveOptionsLayout->addWidget(new QLabel("Quality:", this));
    qualitySlider = new QSlider(Qt::Horizontal, this);
    qualitySlider->setRange(0, 100);
    qualitySlider->setValue(75);
    saveOptionsLayout->addWidget(qualitySlider);
    
    qualitySpinBox = new QSpinBox(this);
    qualitySpinBox->setRange(0, 100);
    qualitySpinBox->setValue(75);
    saveOptionsLayout->addWidget(qualitySpinBox);
    
    saveOptionsLayout->addStretch();
    
    mainLayout->addLayout(saveOptionsLayout);

    // Server controls section
    QHBoxLayout* serverLayout = new QHBoxLayout();
    serverLayout->addWidget(new QLabel("Server:", this));
    
    serverStartButton = new QPushButton("Start Server", this);
    serverStopButton = new QPushButton("Stop Server", this);
    serverStopButton->setEnabled(false);
    
    serverLayout->addWidget(serverStartButton);
    serverLayout->addWidget(serverStopButton);
    
    serverLayout->addWidget(new QLabel("Mode:", this));
    serverModeComboBox = new QComboBox(this);
    serverModeComboBox->addItem("Normal", ServerManager::Normal);
    serverModeComboBox->addItem("Secure (SSL)", ServerManager::Secure);
    serverModeComboBox->setFixedWidth(120);
    serverLayout->addWidget(serverModeComboBox);
    
    serverLayout->addWidget(new QLabel("Port:", this));
    serverPortEdit = new QLineEdit(this);
    serverPortEdit->setPlaceholderText("12345");
    serverPortEdit->setText("12345");
    serverPortEdit->setFixedWidth(80);
    serverLayout->addWidget(serverPortEdit);
    
    serverLayout->addWidget(new QLabel("Image:", this));
    serverImagePathEdit = new QLineEdit(this);
    serverImagePathEdit->setPlaceholderText("Select image file...");
    serverImagePathEdit->setReadOnly(true);
    serverLayout->addWidget(serverImagePathEdit);
    
    serverImagePathButton = new QPushButton("Browse...", this);
    serverImagePathButton->setFixedWidth(80);
    serverLayout->addWidget(serverImagePathButton);
    
    serverProgressiveCheckBox = new QCheckBox("Progressive", this);
    serverLayout->addWidget(serverProgressiveCheckBox);
    
    serverStatusLabel = new QLabel("Stopped", this);
    serverStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    serverLayout->addWidget(serverStatusLabel);
    
    serverLayout->addStretch();
    mainLayout->addLayout(serverLayout);
    
    mainLayout->addStretch();

    connect(loadButton, &QPushButton::clicked, this, &MainWindow::onLoadButtonClicked);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::onSaveButtonClicked);
    connect(nextScanButton, &QPushButton::clicked, this, &MainWindow::onNextScanButtonClicked);
    connect(qualitySlider, &QSlider::valueChanged, this, &MainWindow::onQualityChanged);
    connect(qualitySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            qualitySlider, &QSlider::setValue);
    connect(qualitySlider, &QSlider::valueChanged, 
            qualitySpinBox, &QSpinBox::setValue);

    // Network clients connections
    connect(networkLoadButton, &QPushButton::clicked, this, &MainWindow::onNetworkLoadButtonClicked);
    connect(networkClient, &JPEGClient::imageReceived, this, &MainWindow::onNetworkImageReceived);
    connect(networkClient, &JPEGClient::errorOccurred, this, &MainWindow::onNetworkError);
    connect(networkClient, &JPEGClient::uploadFinished, this, &MainWindow::onUploadFinished);
    connect(networkSslClient, &JPEGSslClient::imageReceived, this, &MainWindow::onNetworkImageReceived);
    connect(networkSslClient, &JPEGSslClient::errorOccurred, this, &MainWindow::onNetworkError);
    connect(networkSslClient, &JPEGSslClient::uploadFinished, this, &MainWindow::onUploadFinished);
    connect(uploadButton, &QPushButton::clicked, this, &MainWindow::onUploadButtonClicked);

    // Server manager connections
    connect(serverStartButton, &QPushButton::clicked, this, &MainWindow::onServerStartButtonClicked);
    connect(serverStopButton, &QPushButton::clicked, this, &MainWindow::onServerStopButtonClicked);
    connect(serverImagePathButton, &QPushButton::clicked, this, &MainWindow::onServerImagePathButtonClicked);
    connect(serverManager, &ServerManager::serverStarted, this, &MainWindow::onServerStarted);
    connect(serverManager, &ServerManager::serverStopped, this, &MainWindow::onServerStopped);
    connect(serverManager, &ServerManager::serverError, this, &MainWindow::onServerError);
}

void MainWindow::onNetworkLoadButtonClicked()
{
    QString ip = ipEdit->text().trimmed();
    bool ok = false;
    quint16 port = portEdit->text().toUShort(&ok);
    if (ip.isEmpty() || !ok) {
        QMessageBox::warning(this, "Network", "Enter valid IP and port");
        return;
    }
    
    bool secureMode = (clientModeComboBox->currentData().toInt() == 1);
    statusBar()->showMessage(QString("Connecting to %1 server...").arg(secureMode ? "secure" : "normal"), 2000);
    
    if (secureMode) {
        networkSslClient->getImage(ip, port);
    } else {
        networkClient->getImage(ip, port);
    }
}

void MainWindow::onNetworkImageReceived(const QImage& image)
{
    currentImage = image;
    updateImageDisplay(image);
    statusBar()->showMessage("Image received from server", 2000);
}

void MainWindow::onNetworkError(const QString& error)
{
    QMessageBox::critical(this, "Network error", error);
    statusBar()->showMessage("Network error", 2000);
}

void MainWindow::onLoadButtonClicked()
{
    QString filename = QFileDialog::getOpenFileName(this, 
        "Load JPEG Image", "", "JPEG Images (*.jpg *.jpeg)");
    
    if (filename.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(filename);
    ImageHandler::HandlerType handlerType = ImageHandler::Progressive;

    if (imageHandler) {
        delete imageHandler;
    }
    imageHandler = ImageHandler::createHandler(handlerType);

    if (loadCommand) {
        delete loadCommand;
    }
    loadCommand = new LoadImageCommand(imageHandler, filename, this);
    loadCommand->execute();
    
    updateNextScanButton();
}

void MainWindow::onUploadButtonClicked()
{
    QString ip = ipEdit->text().trimmed();
    bool ok = false;
    quint16 port = portEdit->text().toUShort(&ok);
    if (ip.isEmpty() || !ok) {
        QMessageBox::warning(this, "Network", "Enter valid IP and port");
        return;
    }

    QString filename = QFileDialog::getOpenFileName(this,
        "Select JPEG to upload", "", "JPEG Images (*.jpg *.jpeg)");
    if (filename.isEmpty())
        return;

    bool secureMode = (clientModeComboBox->currentData().toInt() == 1);
    statusBar()->showMessage(QString("Uploading image to %1 server...").arg(secureMode ? "secure" : "normal"), 2000);
    
    if (secureMode) {
        networkSslClient->uploadImage(ip, port, filename);
    } else {
        networkClient->uploadImage(ip, port, filename);
    }
}

void MainWindow::onUploadFinished(bool success, const QString& message)
{
    if (success) {
        statusBar()->showMessage("Upload successful", 3000);
        QMessageBox::information(this, "Upload", message);
    } else {
        statusBar()->showMessage("Upload failed", 3000);
        QMessageBox::critical(this, "Upload error", message);
    }
}

void MainWindow::onSaveButtonClicked()
{
    if (currentImage.isNull()) {
        QMessageBox::warning(this, "Warning", "No image to save");
        return;
    }
    
    QString filename = QFileDialog::getSaveFileName(this, 
        "Save JPEG Image", "", "JPEG Images (*.jpg *.jpeg)");
    
    if (filename.isEmpty()) {
        return;
    }
    
    int quality = qualitySlider->value();
    bool progressive = progressiveCheckBox->isChecked();
    int dctMethod = dctComboBox->currentData().toInt();
    
    SaveImageCommand saveCommand(imageHandler, filename, currentImage, 
                                 quality, progressive, dctMethod);
    
    if (saveCommand.execute()) {
        QMessageBox::information(this, "Success", "Image saved successfully");
    } else {
        QMessageBox::critical(this, "Error", "Failed to save image");
    }
}

void MainWindow::onNextScanButtonClicked()
{
    if (loadCommand && loadCommand->canLoadNextScan()) {
        loadCommand->executeNextScan();
        updateNextScanButton();
        statusBar()->showMessage(QString("Loaded next scan. Click '>' to load more."), 2000);
    } else {
        statusBar()->showMessage("No more scans available", 2000);
    }
}

void MainWindow::onQualityChanged(int value)
{
    Q_UNUSED(value);
}

void MainWindow::onImageLoaded(const QImage& image)
{
    currentImage = image;
    updateImageDisplay(image);
    updateNextScanButton();
}

void MainWindow::onLoadError(const QString& error)
{
    QMessageBox::critical(this, "Error", error);
    currentImage = QImage();
    updateImageDisplay(QImage());
    updateNextScanButton();
}

void MainWindow::updateImageDisplay(const QImage& image)
{
    if (image.isNull()) {
        imageLabel->setText("No image loaded");
        imageLabel->setPixmap(QPixmap());
        return;
    }
    
    QPixmap pixmap = QPixmap::fromImage(image);
    QSize labelSize = imageLabel->size();
    QSize scaledSize = pixmap.size();
    scaledSize.scale(labelSize, Qt::KeepAspectRatio);
    
    if (scaledSize != pixmap.size()) {
        pixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    imageLabel->setPixmap(pixmap);
    imageLabel->setText("");
}

void MainWindow::updateNextScanButton()
{
    if (loadCommand && imageHandler) {
        bool canLoad = loadCommand->canLoadNextScan();
        nextScanButton->setEnabled(canLoad);
    } else {
        nextScanButton->setEnabled(false);
    }
}

void MainWindow::onServerStartButtonClicked()
{
    QString imagePath = serverImagePathEdit->text().trimmed();
    if (imagePath.isEmpty()) {
        QMessageBox::warning(this, "Server", "Please select an image file for the server");
        return;
    }

    bool ok;
    quint16 port = serverPortEdit->text().toUShort(&ok);
    if (!ok || port == 0) {
        QMessageBox::warning(this, "Server", "Please enter a valid port number");
        return;
    }

    ServerManager::ServerMode mode = static_cast<ServerManager::ServerMode>(
        serverModeComboBox->currentData().toInt());
    bool progressive = serverProgressiveCheckBox->isChecked();

    if (serverManager->startServer(mode, port, imagePath, progressive)) {
        statusBar()->showMessage(QString("Server starting on port %1...").arg(port), 2000);
    } else {
        QMessageBox::critical(this, "Server", "Failed to start server. Check the console for details.");
    }
}

void MainWindow::onServerStopButtonClicked()
{
    serverManager->stopServer();
    statusBar()->showMessage("Server stopping...", 2000);
}

void MainWindow::onServerImagePathButtonClicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
        "Select Image for Server", "", "JPEG Images (*.jpg *.jpeg)");
    if (!filename.isEmpty()) {
        serverImagePathEdit->setText(filename);
    }
}

void MainWindow::onServerStarted(quint16 port, ServerManager::ServerMode mode)
{
    QString modeStr = (mode == ServerManager::Secure) ? "Secure" : "Normal";
    serverStatusLabel->setText(QString("Running (%1) on port %2").arg(modeStr).arg(port));
    serverStatusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    serverStartButton->setEnabled(false);
    serverStopButton->setEnabled(true);
    serverPortEdit->setEnabled(false);
    serverModeComboBox->setEnabled(false);
    serverImagePathButton->setEnabled(false);
    statusBar()->showMessage(QString("Server started on port %1 (%2 mode)").arg(port).arg(modeStr), 3000);
}

void MainWindow::onServerStopped()
{
    serverStatusLabel->setText("Stopped");
    serverStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    serverStartButton->setEnabled(true);
    serverStopButton->setEnabled(false);
    serverPortEdit->setEnabled(true);
    serverModeComboBox->setEnabled(true);
    serverImagePathButton->setEnabled(true);
    statusBar()->showMessage("Server stopped", 2000);
}

void MainWindow::onServerError(const QString& error)
{
    QMessageBox::critical(this, "Server Error", error);
    statusBar()->showMessage("Server error: " + error, 3000);
}

void MainWindow::updateServerControls()
{
    bool running = serverManager && serverManager->isRunning();
    serverStartButton->setEnabled(!running);
    serverStopButton->setEnabled(running);
    serverPortEdit->setEnabled(!running);
    serverModeComboBox->setEnabled(!running);
    serverImagePathButton->setEnabled(!running);
}


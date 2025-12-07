#include "servermanager.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QStandardPaths>

ServerManager::ServerManager(QObject* parent)
    : QObject(parent)
    , serverProcess(nullptr)
    , currentMode(Normal)
    , currentPort(0)
{
}

ServerManager::~ServerManager()
{
    stopServer();
}

QString ServerManager::getServerExecutableName(ServerMode mode) const
{
    if (mode == Secure) {
        return "jpeg_server_secure";
    }
    return "jpeg_server";
}

QString ServerManager::serverExecutablePath()
{
    QString serverExe = getServerExecutableName(currentMode);
    
#ifdef Q_OS_WIN
    serverExe += ".exe";
#endif

    QString appDir = QCoreApplication::applicationDirPath();
    QString path = QDir(appDir).filePath(serverExe);
    if (QFile::exists(path)) {
        qDebug() << "Found server executable in app directory:" << path;
        return path;
    }

    QString buildDir = QDir::currentPath();
    path = QDir(buildDir).filePath(serverExe);
    if (QFile::exists(path)) {
        qDebug() << "Found server executable in build directory:" << path;
        return path;
    }

    QStringList subdirs = {"debug", "release", "Debug", "Release"};
    for (const QString& subdir : subdirs) {
        path = QDir(buildDir).filePath(subdir + "/" + serverExe);
        if (QFile::exists(path)) {
            qDebug() << "Found server executable in subdirectory:" << path;
            return path;
        }
    }

    QString projectDir = QDir::currentPath();
    path = QDir(projectDir).filePath(serverExe);
    if (QFile::exists(path)) {
        qDebug() << "Found server executable in project directory:" << path;
        return path;
    }

    qWarning() << "Server executable not found, will search in PATH:" << serverExe;
    qWarning() << "Searched in:" << appDir << buildDir << projectDir;
    return serverExe;
}

bool ServerManager::startServer(ServerMode mode, quint16 port, const QString& imagePath, bool progressive)
{
    if (isRunning()) {
        qWarning() << "Server is already running";
        return false;
    }

    if (port == 0 || port > 65535) {
        emit serverError("Invalid port number");
        return false;
    }

    if (imagePath.isEmpty() || !QFile::exists(imagePath)) {
        emit serverError("Image file does not exist: " + imagePath);
        return false;
    }

    currentMode = mode;
    currentPort = port;
    currentImagePath = imagePath;

    if (!serverProcess) {
        serverProcess = new QProcess(this);
        connect(serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ServerManager::onProcessFinished);
        connect(serverProcess, &QProcess::errorOccurred,
                this, &ServerManager::onProcessError);
        connect(serverProcess, &QProcess::readyReadStandardOutput,
                this, &ServerManager::onProcessReadyRead);
        connect(serverProcess, &QProcess::readyReadStandardError,
                this, &ServerManager::onProcessReadyRead);
    }

    QString executable = serverExecutablePath();
    QStringList arguments;
    
    if (progressive) {
        arguments << "-g" << "--progressive";
    }
    arguments << "-p" << QString::number(port);
    arguments << imagePath;

    qDebug() << "Starting server:" << executable << arguments;
    
    if (!QFile::exists(executable) && !executable.contains("/") && !executable.contains("\\")) {
        QString error = QString("Server executable not found: %1\n\n"
                               "Please build the server project first:\n"
                               "1. Open jpeg_server.pro in Qt Creator\n"
                               "2. Build the project\n"
                               "3. The executable should be in the build directory")
                               .arg(executable);
        qWarning() << error;
        emit serverError(error);
        return false;
    }
    
    serverProcess->start(executable, arguments);

    if (!serverProcess->waitForStarted(3000)) {
        QString error = QString("Failed to start server: %1\n\n"
                               "Executable: %2\n"
                               "Make sure the server is built and accessible.")
                               .arg(serverProcess->errorString())
                               .arg(executable);
        qWarning() << error;
        emit serverError(error);
        return false;
    }

    qDebug() << "Server started successfully";
    emit serverStarted(port, mode);
    return true;
}

void ServerManager::stopServer()
{
    if (serverProcess && serverProcess->state() != QProcess::NotRunning) {
        qDebug() << "Stopping server...";
        serverProcess->terminate();
        if (!serverProcess->waitForFinished(3000)) {
            qWarning() << "Server did not terminate, killing...";
            serverProcess->kill();
            serverProcess->waitForFinished(1000);
        }
        emit serverStopped();
    }
}

bool ServerManager::isRunning() const
{
    return serverProcess && serverProcess->state() == QProcess::Running;
}

void ServerManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit) {
        QString error = "Server crashed";
        qWarning() << error;
        emit serverError(error);
    } else if (exitCode != 0) {
        QString error = QString("Server exited with code %1").arg(exitCode);
        qWarning() << error;
        emit serverError(error);
    } else {
        qDebug() << "Server stopped normally";
    }
    emit serverStopped();
}

void ServerManager::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Server failed to start. Make sure the server executable exists.";
            break;
        case QProcess::Crashed:
            errorMsg = "Server crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "Server operation timed out";
            break;
        case QProcess::WriteError:
            errorMsg = "Error writing to server process";
            break;
        case QProcess::ReadError:
            errorMsg = "Error reading from server process";
            break;
        default:
            errorMsg = "Unknown server error";
            break;
    }
    qWarning() << "Server process error:" << errorMsg;
    emit serverError(errorMsg);
}

void ServerManager::onProcessReadyRead()
{
    QByteArray data = serverProcess->readAllStandardOutput();
    if (!data.isEmpty()) {
        QString output = QString::fromUtf8(data).trimmed();
        qDebug() << "Server output:" << output;
        emit serverOutput(output);
    }

    QByteArray errorData = serverProcess->readAllStandardError();
    if (!errorData.isEmpty()) {
        QString error = QString::fromUtf8(errorData).trimmed();
        qWarning() << "Server error output:" << error;
        emit serverError(error);
    }
}


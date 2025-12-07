#ifndef SERVERMANAGER_H
#define SERVERMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

/**
 * ServerManager - Facade Pattern для управления серверным процессом
 * Упрощает запуск, остановку и мониторинг состояния JPEG сервера
 */
class ServerManager : public QObject
{
    Q_OBJECT

public:
    enum ServerMode {
        Normal,      // Обычный сервер (QTcpServer)
        Secure       // Безопасный сервер (QSslServer)
    };

    explicit ServerManager(QObject* parent = nullptr);
    ~ServerManager();

    bool startServer(ServerMode mode, quint16 port, const QString& imagePath, bool progressive = false);
    void stopServer();
    bool isRunning() const;
    quint16 getPort() const { return currentPort; }
    ServerMode getMode() const { return currentMode; }
    QString getImagePath() const { return currentImagePath; }

signals:
    void serverStarted(quint16 port, ServerMode mode);
    void serverStopped();
    void serverError(const QString& error);
    void serverOutput(const QString& output);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessReadyRead();

private:
    QProcess* serverProcess;
    ServerMode currentMode;
    quint16 currentPort;
    QString currentImagePath;
    QString serverExecutablePath();

    QString getServerExecutableName(ServerMode mode) const;
};

#endif // SERVERMANAGER_H


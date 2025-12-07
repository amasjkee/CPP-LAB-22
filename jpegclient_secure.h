#ifndef JPEGCLIENT_SECURE_H
#define JPEGCLIENT_SECURE_H

#include <QSslSocket>
#include <QObject>
#include <QImage>

class JPEGSslClient : public QObject {
    Q_OBJECT
public:
    explicit JPEGSslClient(QObject* parent = nullptr);
    void getImage(const QString& host, quint16 port);
    void uploadImage(const QString& host, quint16 port, const QString& filename);
    QImage getLastImage() const;
signals:
    void imageReceived(const QImage& image);
    void errorOccurred(const QString& error);
    void uploadFinished(bool success, const QString& message);
private slots:
    void onReadyRead();
    void onEncrypted();
    void onSslErrors(const QList<QSslError>& errors);
    void onError(QAbstractSocket::SocketError);
private:
    QSslSocket* socket;
    QByteArray buffer;
    QImage lastImage;
    bool headerParsed;
    int contentLength;
    enum OperationMode { None, GetImage, UploadImage };
    OperationMode mode;
    QByteArray uploadBuffer;
};

#endif // JPEGCLIENT_SECURE_H

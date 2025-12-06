#ifndef JPEGCLIENT_H
#define JPEGCLIENT_H

#include <QtNetwork/QTcpSocket>
#include <QObject>
#include <QImage>

class JPEGClient : public QObject {
    Q_OBJECT
public:
    explicit JPEGClient(QObject* parent = nullptr);
    void getImage(const QString& host, quint16 port);
    void uploadImage(const QString& host, quint16 port, const QString& filename);
    QImage getLastImage() const;
signals:
    void imageReceived(const QImage& image);
    void errorOccurred(const QString& error);
    void uploadFinished(bool success, const QString& message);
private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError);
private:
    QTcpSocket* socket;
    QByteArray buffer;
    QImage lastImage;
    bool headerParsed;
    int contentLength;
    enum OperationMode { None, GetImage, UploadImage };
    OperationMode mode;
    QByteArray uploadBuffer;
};

#endif // JPEGCLIENT_H

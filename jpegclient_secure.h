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
    QImage getLastImage() const;
signals:
    void imageReceived(const QImage& image);
    void errorOccurred(const QString& error);
private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError);
private:
    QSslSocket* socket;
    QByteArray buffer;
    QImage lastImage;
    bool headerParsed;
    int contentLength;
};

#endif // JPEGCLIENT_SECURE_H

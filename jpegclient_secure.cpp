#include "jpegclient_secure.h"
#include <QHostAddress>
#include <QBuffer>
#include <QImageReader>

JPEGSslClient::JPEGSslClient(QObject* parent)
    : QObject(parent), socket(new QSslSocket(this)), headerParsed(false), contentLength(0) {
    connect(socket, &QSslSocket::readyRead, this, &JPEGSslClient::onReadyRead);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
            this, &JPEGSslClient::onError);
}

void JPEGSslClient::getImage(const QString& host, quint16 port) {
    buffer.clear();
    headerParsed = false;
    contentLength = 0;
    socket->connectToHostEncrypted(host, port);
    // socket->setCaCertificates(...); // при необходимости
    if (socket->waitForEncrypted(3000)) {
        QByteArray request = "GET / HTTP/1.1\r\nHost: " + host.toUtf8() + "\r\n\r\n";
        socket->write(request);
    } else {
        emit errorOccurred("SSL connection failed");
    }
}

void JPEGSslClient::onReadyRead() {
    buffer += socket->readAll();
    if (!headerParsed) {
        int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd != -1) {
            QByteArray header = buffer.left(headerEnd);
            buffer = buffer.mid(headerEnd + 4);
            QList<QByteArray> lines = header.split('\n');
            for (const QByteArray& line : lines) {
                if (line.startsWith("Content-Length:")) {
                    contentLength = line.mid(15).trimmed().toInt();
                }
            }
            headerParsed = true;
        }
    }
    if (headerParsed && buffer.size() >= contentLength && contentLength > 0) {
        QImage img;
        img.loadFromData(buffer.left(contentLength), "JPEG");
        lastImage = img;
        emit imageReceived(img);
        socket->disconnectFromHost();
    }
}

void JPEGSslClient::onError(QAbstractSocket::SocketError) {
    emit errorOccurred(socket->errorString());
}

QImage JPEGSslClient::getLastImage() const {
    return lastImage;
}

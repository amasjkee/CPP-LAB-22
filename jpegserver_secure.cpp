#include "jpegserver_secure.h"
#include <QtNetwork/QHostAddress>
#include <QFile>
#include <QImage>
#include <QBuffer>
#include <QDebug>

JPEGSslServer::JPEGSslServer(QObject* parent)
    : QSslServer(parent), strategy(nullptr) {}

void JPEGSslServer::setStrategy(JPEGStrategy* s) {
    strategy = s;
}

void JPEGSslServer::setImagePath(const QString& path) {
    imagePath = path;
}

void JPEGSslServer::incomingConnection(qintptr socketDescriptor) {
    QSslSocket* socket = new QSslSocket(this);
    socket->setSocketDescriptor(socketDescriptor);
    // Здесь нужно загрузить сертификаты ГОСТ
    // socket->setPrivateKey("gost_private.key");
    // socket->setLocalCertificate("gost_cert.crt");
    socket->startServerEncryption();
    connect(socket, &QSslSocket::readyRead, [this, socket]() {
        QByteArray request = socket->readAll();
        if (request.startsWith("GET ")) {
            QImage image;
            if (strategy && strategy->loadImage(imagePath, image)) {
                QByteArray ba;
                QBuffer buffer(&ba);
                buffer.open(QIODevice::WriteOnly);
                image.save(&buffer, "JPEG");
                QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: "
                    + QByteArray::number(ba.size()) + "\r\n\r\n" + ba;
                socket->write(response);
            } else {
                QByteArray response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
                socket->write(response);
            }
        }
        socket->disconnectFromHost();
    });
    connect(socket, &QSslSocket::disconnected, socket, &QSslSocket::deleteLater);
}

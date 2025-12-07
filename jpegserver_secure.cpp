#include "jpegserver_secure.h"
#include <QtNetwork/QHostAddress>
#include <QFile>
#include <QImage>
#include <QBuffer>
#include <QDebug>
#include <QSslKey>
#include <QSslCertificate>

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
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        qWarning() << "Failed to set socket descriptor:" << socket->errorString();
        delete socket;
        return;
    }

    QByteArray* accum = new QByteArray();
    bool* requestProcessed = new bool(false);
    bool* sslEncrypted = new bool(false);
    int* expectedContentLength = new int(-1);

    connect(socket, &QSslSocket::encrypted, [socket, sslEncrypted]() {
        *sslEncrypted = true;
        qDebug() << "SSL encryption established";
    });

    connect(socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
            [socket](const QList<QSslError>& errors) {
        qWarning() << "SSL errors occurred:";
        for (const QSslError& error : errors) {
            qWarning() << "  -" << error.errorString();
        }
        socket->ignoreSslErrors();
    });

    socket->startServerEncryption();

    connect(socket, &QSslSocket::readyRead, [this, socket, accum, requestProcessed, sslEncrypted, expectedContentLength]() {
        if (!*sslEncrypted) {
            qDebug() << "Waiting for SSL encryption...";
            return;
        }

        if (*requestProcessed) {
            return;
        }

        accum->append(socket->readAll());

        int headerEnd = accum->indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            return;
        }

        QByteArray header = accum->left(headerEnd);
        QByteArray body = accum->mid(headerEnd + 4);

        qDebug() << "Incoming secure request header:" << header.left(200);

        if (header.startsWith("GET ")) {
            *requestProcessed = true;
            QImage image;
            if (strategy && !imagePath.isEmpty() && strategy->loadImage(imagePath, image)) {
                QByteArray ba;
                QBuffer buffer(&ba);
                buffer.open(QIODevice::WriteOnly);
                if (image.save(&buffer, "JPEG")) {
                    QByteArray response = "HTTP/1.1 200 OK\r\n"
                                         "Content-Type: image/jpeg\r\n"
                                         "Content-Length: " + QByteArray::number(ba.size()) + "\r\n"
                                         "Connection: close\r\n\r\n" + ba;
                    socket->write(response);
                    qDebug() << "Sent secure image response, size:" << ba.size();
                } else {
                    QByteArray response = "HTTP/1.1 500 Internal Server Error\r\n"
                                         "Content-Length: 0\r\n\r\n";
                    socket->write(response);
                    qWarning() << "Failed to save image to buffer";
                }
            } else {
                QByteArray response = "HTTP/1.1 404 Not Found\r\n"
                                     "Content-Length: 0\r\n\r\n";
                socket->write(response);
                qWarning() << "Image not found or failed to load:" << imagePath;
            }
            socket->flush();
            socket->disconnectFromHost();
            return;
        }

        if (header.startsWith("POST ")) {
            if (*expectedContentLength < 0) {
                QList<QByteArray> lines = header.split('\n');
                for (const QByteArray& line : lines) {
                    QByteArray trimmedLine = line.trimmed();
                    if (trimmedLine.toLower().startsWith("content-length:")) {
                        QByteArray val = trimmedLine.mid(15).trimmed();
                        bool ok;
                        *expectedContentLength = val.toInt(&ok);
                        if (!ok || *expectedContentLength < 0) {
                            *expectedContentLength = 0;
                        }
                        break;
                    }
                }
            }

            if (*expectedContentLength <= 0) {
                *requestProcessed = true;
                QByteArray response = "HTTP/1.1 400 Bad Request\r\n"
                                     "Content-Length: 0\r\n\r\n";
                socket->write(response);
                socket->disconnectFromHost();
                qWarning() << "Invalid or missing Content-Length in POST request";
                return;
            }

            if (body.size() < *expectedContentLength) {
                qDebug() << "Waiting for more body bytes: have" << body.size() 
                         << "need" << *expectedContentLength;
                return;
            }

            *requestProcessed = true;
            QByteArray imageData = body.left(*expectedContentLength);
            QImage img;
            
            if (!img.loadFromData(imageData, "JPEG")) {
                QByteArray response = "HTTP/1.1 415 Unsupported Media Type\r\n"
                                     "Content-Length: 0\r\n\r\n";
                socket->write(response);
                socket->disconnectFromHost();
                qWarning() << "Failed to load image from POST data";
                return;
            }

            bool saved = false;
            if (!imagePath.isEmpty()) {
                saved = img.save(imagePath, "JPEG");
                qDebug() << "Save result:" << saved << "to" << imagePath;
            } else {
                qWarning() << "Image path is empty, cannot save uploaded image";
            }

            if (saved) {
                QByteArray response = "HTTP/1.1 200 OK\r\n"
                                     "Content-Length: 0\r\n\r\n";
                socket->write(response);
            } else {
                QByteArray response = "HTTP/1.1 500 Internal Server Error\r\n"
                                     "Content-Length: 0\r\n\r\n";
                socket->write(response);
            }
            socket->flush();
            socket->disconnectFromHost();
            return;
        }

        *requestProcessed = true;
        QByteArray response = "HTTP/1.1 400 Bad Request\r\n"
                             "Content-Length: 0\r\n\r\n";
        socket->write(response);
        socket->disconnectFromHost();
        qWarning() << "Unknown HTTP method in request";
    });

    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
            [socket](QAbstractSocket::SocketError error) {
        qWarning() << "Socket error:" << error << socket->errorString();
    });

    connect(socket, &QSslSocket::disconnected, [socket, accum, requestProcessed, sslEncrypted, expectedContentLength]() {
        delete accum;
        delete requestProcessed;
        delete sslEncrypted;
        delete expectedContentLength;
        socket->deleteLater();
    });
}

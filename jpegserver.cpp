#include "jpegserver.h"
#include <QtNetwork/QHostAddress>
#include <QFile>
#include <QImage>
#include <QBuffer>
#include <QDebug>

JPEGServer::JPEGServer(QObject* parent)
    : QTcpServer(parent), strategy(nullptr) {}

void JPEGServer::setStrategy(JPEGStrategy* s) {
    strategy = s;
}

void JPEGServer::setImagePath(const QString& path) {
    imagePath = path;
}

void JPEGServer::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket* socket = new QTcpSocket(this);
    socket->setSocketDescriptor(socketDescriptor);
    // Переменная для накопления данных по сокету
    QByteArray* accum = new QByteArray();
    connect(socket, &QTcpSocket::readyRead, [this, socket, accum]() {
        accum->append(socket->readAll());

        // Ждем конца заголовка
        int headerEnd = accum->indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            return; // ждем полного заголовка
        }

        QByteArray header = accum->left(headerEnd);
        QByteArray body = accum->mid(headerEnd + 4);

        // Простая проверка на метод
        qDebug() << "Incoming request header:" << header.left(200);
        if (header.startsWith("GET ")) {
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
            socket->disconnectFromHost();
            return;
        }

        if (header.startsWith("POST ")) {
            qDebug() << "POST request received, body bytes:" << body.size();
            // Найдем Content-Length
            int contentLength = 0;
            QList<QByteArray> lines = header.split('\n');
            for (const QByteArray& line : lines) {
                if (line.trimmed().toLower().startsWith("content-length:")) {
                    QByteArray val = line.mid(line.indexOf(':') + 1).trimmed();
                    contentLength = val.toInt();
                    break;
                }
            }

            if (contentLength <= 0) {
                QByteArray response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
                socket->write(response);
                socket->disconnectFromHost();
                return;
            }

            // Если телo полностью пришло
            if (body.size() < contentLength) {
                qDebug() << "Waiting for more body bytes: have" << body.size() << "need" << contentLength;
                return; // дождемся следующего readyRead
            }

            QByteArray imageData = body.left(contentLength);
            QImage img;
            if (!img.loadFromData(imageData)) {
                QByteArray response = "HTTP/1.1 415 Unsupported Media Type\r\nContent-Length: 0\r\n\r\n";
                socket->write(response);
                socket->disconnectFromHost();
                return;
            }

            // Сохраняем изображение
            bool saved = false;
            if (!imagePath.isEmpty()) {
                saved = img.save(imagePath, "JPEG");
            }
            qDebug() << "Save result:" << saved << "to" << imagePath;
            if (saved) {
                QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
                socket->write(response);
            } else {
                QByteArray response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
                socket->write(response);
            }
            socket->disconnectFromHost();
            return;
        }

        // Неизвестный метод
        QByteArray response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        socket->write(response);
        socket->disconnectFromHost();
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    // очистим накопитель при удалении сокета
    connect(socket, &QTcpSocket::disconnected, [accum]() { delete accum; });
}

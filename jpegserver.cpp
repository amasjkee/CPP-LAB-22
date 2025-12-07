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
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        qWarning() << "Failed to set socket descriptor:" << socket->errorString();
        delete socket;
        return;
    }

    // Переменные для накопления данных по сокету и отслеживания состояния
    QByteArray* accum = new QByteArray();
    bool* requestProcessed = new bool(false);
    int* expectedContentLength = new int(-1);

    connect(socket, &QTcpSocket::readyRead, [this, socket, accum, requestProcessed, expectedContentLength]() {
        // Если запрос уже обработан, игнорируем новые данные
        if (*requestProcessed) {
            return;
        }

        accum->append(socket->readAll());

        // Ждем конца заголовка
        int headerEnd = accum->indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            return; // ждем полного заголовка
        }

        QByteArray header = accum->left(headerEnd);
        QByteArray body = accum->mid(headerEnd + 4);

        qDebug() << "Incoming request header:" << header.left(200);

        // Обработка GET запроса
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
                    qDebug() << "Sent image response, size:" << ba.size();
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

        // Обработка POST запроса
        if (header.startsWith("POST ")) {
            // Если Content-Length еще не извлечен, извлекаем его
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

            // Проверяем, полностью ли пришло тело запроса
            if (body.size() < *expectedContentLength) {
                qDebug() << "Waiting for more body bytes: have" << body.size() 
                         << "need" << *expectedContentLength;
                return; // дождемся следующего readyRead
            }

            // Тело полностью получено, обрабатываем
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

            // Сохраняем изображение
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

        // Неизвестный метод
        *requestProcessed = true;
        QByteArray response = "HTTP/1.1 400 Bad Request\r\n"
                             "Content-Length: 0\r\n\r\n";
        socket->write(response);
        socket->disconnectFromHost();
        qWarning() << "Unknown HTTP method in request";
    });

    // Обработка ошибок сокета
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            [socket](QAbstractSocket::SocketError error) {
        qWarning() << "Socket error:" << error << socket->errorString();
    });

    // Очистка при отключении
    connect(socket, &QTcpSocket::disconnected, [socket, accum, requestProcessed, expectedContentLength]() {
        delete accum;
        delete requestProcessed;
        delete expectedContentLength;
        socket->deleteLater();
    });
}

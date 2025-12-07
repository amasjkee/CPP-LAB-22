#include "jpegclient.h"
#include <QtNetwork/QHostAddress>
#include <QBuffer>
#include <QImageReader>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

JPEGClient::JPEGClient(QObject* parent)
    : QObject(parent), socket(new QTcpSocket(this)), headerParsed(false), contentLength(0), mode(None) {
    connect(socket, &QTcpSocket::readyRead, this, &JPEGClient::onReadyRead);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &JPEGClient::onError);
    connect(socket, &QTcpSocket::connected, [this]() {
        qDebug() << "Connected to server";
    });
    connect(socket, &QTcpSocket::disconnected, [this]() {
        qDebug() << "Disconnected from server";
    });
}

void JPEGClient::getImage(const QString& host, quint16 port) {
    // Валидация входных данных
    if (host.isEmpty()) {
        emit errorOccurred("Host address is empty");
        return;
    }
    if (port == 0 || port > 65535) {
        emit errorOccurred("Invalid port number");
        return;
    }

    buffer.clear();
    headerParsed = false;
    contentLength = 0;
    uploadBuffer.clear();
    mode = GetImage;

    qDebug() << "Connecting to" << host << ":" << port;
    socket->abort();
    socket->connectToHost(host, port);
    
    if (!socket->waitForConnected(5000)) {
        QString errorMsg = QString("Connection failed: %1").arg(socket->errorString());
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        mode = None;
        return;
    }

    QByteArray request = "GET / HTTP/1.1\r\n"
                        "Host: " + host.toUtf8() + "\r\n"
                        "Connection: close\r\n"
                        "\r\n";
    
    qint64 written = socket->write(request);
    if (written != request.size()) {
        QString errorMsg = QString("Failed to send request: %1").arg(socket->errorString());
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        socket->disconnectFromHost();
        mode = None;
        return;
    }

    if (!socket->waitForBytesWritten(3000)) {
        QString errorMsg = QString("Write timeout: %1").arg(socket->errorString());
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        socket->disconnectFromHost();
        mode = None;
        return;
    }

    qDebug() << "GET request sent, waiting for response";
}

void JPEGClient::uploadImage(const QString& host, quint16 port, const QString& filename) {
    if (host.isEmpty()) {
        emit uploadFinished(false, "Host address is empty");
        return;
    }
    if (port == 0 || port > 65535) {
        emit uploadFinished(false, "Invalid port number");
        return;
    }

    QFile file(filename);
    if (!file.exists()) {
        emit uploadFinished(false, "File does not exist: " + filename);
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        emit uploadFinished(false, "Unable to open file for reading: " + filename);
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        emit uploadFinished(false, "File is empty: " + filename);
        return;
    }

    buffer.clear();
    headerParsed = false;
    contentLength = 0;
    uploadBuffer = data;
    mode = UploadImage;

    qDebug() << "Uploading" << filename << "(" << data.size() << "bytes) to" << host << ":" << port;
    socket->abort();
    socket->connectToHost(host, port);
    
    if (!socket->waitForConnected(10000)) {
        mode = None;
        QString errorMsg = QString("Connection failed: %1").arg(socket->errorString());
        qWarning() << errorMsg;
        emit uploadFinished(false, errorMsg);
        return;
    }

    QByteArray request;
    request += "POST / HTTP/1.1\r\n";
    request += "Host: " + host.toUtf8() + "\r\n";
    request += "Content-Type: image/jpeg\r\n";
    request += "Content-Length: " + QByteArray::number(uploadBuffer.size()) + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    
    qint64 written = socket->write(request);
    if (written != request.size()) {
        mode = None;
        QString errorMsg = QString("Write header failed: %1").arg(socket->errorString());
        qWarning() << errorMsg;
        emit uploadFinished(false, errorMsg);
        socket->disconnectFromHost();
        return;
    }
    
    if (!socket->waitForBytesWritten(5000)) {
        mode = None;
        QString errorMsg = QString("Write timeout (header): %1").arg(socket->errorString());
        qWarning() << errorMsg;
        emit uploadFinished(false, errorMsg);
        socket->disconnectFromHost();
        return;
    }

    written = socket->write(uploadBuffer);
    if (written != uploadBuffer.size()) {
        mode = None;
        QString errorMsg = QString("Write body failed: %1").arg(socket->errorString());
        qWarning() << errorMsg;
        emit uploadFinished(false, errorMsg);
        socket->disconnectFromHost();
        return;
    }
    
    if (!socket->waitForBytesWritten(10000)) {
        mode = None;
        QString errorMsg = QString("Write timeout (body): %1").arg(socket->errorString());
        qWarning() << errorMsg;
        emit uploadFinished(false, errorMsg);
        socket->disconnectFromHost();
        return;
    }

    qDebug() << "Upload data sent, waiting for server response";
}

void JPEGClient::onReadyRead() {
    buffer += socket->readAll();

    if (mode == GetImage) {
        if (!headerParsed) {
            int headerEnd = buffer.indexOf("\r\n\r\n");
            if (headerEnd != -1) {
                QByteArray header = buffer.left(headerEnd);
                buffer = buffer.mid(headerEnd + 4);
                QList<QByteArray> lines = header.split('\n');
                for (const QByteArray& line : lines) {
                    QByteArray trimmedLine = line.trimmed();
                    if (trimmedLine.toLower().startsWith("content-length:")) {
                        bool ok;
                        contentLength = trimmedLine.mid(15).trimmed().toInt(&ok);
                        if (!ok) {
                            contentLength = 0;
                        }
                        qDebug() << "Content-Length:" << contentLength;
                    }
                }
                headerParsed = true;
            } else {
                return;
            }
        }
        
        if (headerParsed) {
            if (contentLength > 0) {
                if (buffer.size() >= contentLength) {
                    QImage img;
                    if (img.loadFromData(buffer.left(contentLength), "JPEG")) {
                        lastImage = img;
                        qDebug() << "Image received successfully, size:" << img.size();
                        emit imageReceived(img);
                    } else {
                        qWarning() << "Failed to load image from received data";
                        emit errorOccurred("Failed to decode image data");
                    }
                    socket->disconnectFromHost();
                    mode = None;
                } else {
                    qDebug() << "Received" << buffer.size() << "of" << contentLength << "bytes";
                }
            } else {
                QImage img;
                if (img.loadFromData(buffer, "JPEG")) {
                    lastImage = img;
                    qDebug() << "Image received (no Content-Length), size:" << img.size();
                    emit imageReceived(img);
                    socket->disconnectFromHost();
                    mode = None;
                }
            }
        }
    } else if (mode == UploadImage) {
        int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            return;
        }
        
        QByteArray header = buffer.left(headerEnd);
        QList<QByteArray> lines = header.split('\n');
        if (!lines.isEmpty()) {
            QList<QByteArray> statusParts = lines.first().split(' ');
            if (statusParts.size() >= 2) {
                bool ok;
                int code = statusParts[1].trimmed().toInt(&ok);
                if (ok) {
                    qDebug() << "Server response code:" << code;
                    if (code >= 200 && code < 300) {
                        emit uploadFinished(true, "Upload successful");
                    } else {
                        QString errorMsg = QString("Server responded with code %1").arg(code);
                        qWarning() << errorMsg;
                        emit uploadFinished(false, errorMsg);
                    }
                } else {
                    qWarning() << "Invalid status code in response";
                    emit uploadFinished(false, "Invalid server response format");
                }
            } else {
                qWarning() << "Invalid response header format";
                emit uploadFinished(false, "Invalid server response format");
            }
        } else {
            qWarning() << "Empty response header";
            emit uploadFinished(false, "Invalid server response");
        }
        socket->disconnectFromHost();
        mode = None;
    }
}

void JPEGClient::onError(QAbstractSocket::SocketError error) {
    if (error == QAbstractSocket::RemoteHostClosedError && mode == UploadImage) {
        return;
    }
    
    QString err = socket->errorString();
    qWarning() << "Socket error:" << error << "-" << err;
    
    if (mode == UploadImage) {
        emit uploadFinished(false, QString("Network error: %1").arg(err));
    } else if (mode == GetImage) {
        emit errorOccurred(QString("Network error: %1").arg(err));
    }
    mode = None;
}

QImage JPEGClient::getLastImage() const {
    return lastImage;
}

#include "jpegclient.h"
#include <QtNetwork/QHostAddress>
#include <QBuffer>
#include <QImageReader>
#include <QFile>
#include <QFileInfo>

JPEGClient::JPEGClient(QObject* parent)
    : QObject(parent), socket(new QTcpSocket(this)), headerParsed(false), contentLength(0), mode(None) {
    connect(socket, &QTcpSocket::readyRead, this, &JPEGClient::onReadyRead);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &JPEGClient::onError);
}

void JPEGClient::getImage(const QString& host, quint16 port) {
    // Подготовка для GET
    buffer.clear();
    headerParsed = false;
    contentLength = 0;
    uploadBuffer.clear();
    mode = GetImage;

    socket->abort();
    socket->connectToHost(host, port);
    if (socket->waitForConnected(3000)) {
        QByteArray request = "GET / HTTP/1.1\r\nHost: " + host.toUtf8() + "\r\n\r\n";
        socket->write(request);
    } else {
        emit errorOccurred("Connection failed");
    }
}

void JPEGClient::uploadImage(const QString& host, quint16 port, const QString& filename) {
    QFile file(filename);
    if (!file.exists()) {
        emit uploadFinished(false, "File does not exist");
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        emit uploadFinished(false, "Unable to open file for reading");
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    // Подготовка для POST
    buffer.clear();
    headerParsed = false;
    contentLength = 0;
    uploadBuffer = data;
    mode = UploadImage;

    socket->abort();
    socket->connectToHost(host, port);
    // увеличить таймаут подключения — может быть полезно в сети с задержками
    if (!socket->waitForConnected(10000)) {
        mode = None;
        emit uploadFinished(false, QString("Connection failed: %1").arg(socket->errorString()));
        return;
    }

    QByteArray request;
    request += "POST / HTTP/1.1\r\n";
    request += "Host: " + host.toUtf8() + "\r\n";
    request += "Content-Type: image/jpeg\r\n";
    request += "Content-Length: " + QByteArray::number(uploadBuffer.size()) + "\r\n";
    request += "\r\n";
    // отправляем заголовок + тело
    qint64 written = socket->write(request);
    if (written != request.size()) {
        // попытка записи заголовка не удалась
        mode = None;
        emit uploadFinished(false, QString("Write header failed: %1").arg(socket->errorString()));
        return;
    }
    // дождёмся, пока байты будут отправлены в сокет
    if (!socket->waitForBytesWritten(5000)) {
        mode = None;
        emit uploadFinished(false, QString("Write timeout: %1").arg(socket->errorString()));
        return;
    }

    written = socket->write(uploadBuffer);
    if (written != uploadBuffer.size()) {
        mode = None;
        emit uploadFinished(false, QString("Write body failed: %1").arg(socket->errorString()));
        return;
    }
    if (!socket->waitForBytesWritten(5000)) {
        mode = None;
        emit uploadFinished(false, QString("Write timeout (body): %1").arg(socket->errorString()));
        return;
    }

    // Подождём ответа сервера (если его не будет — выдадим таймаут)
    if (!socket->waitForReadyRead(10000)) {
        // Это не обязательно означает, что соединение упало — но даст понятную диагностику
        mode = None;
        emit uploadFinished(false, QString("No response from server: %1").arg(socket->errorString()));
        socket->disconnectFromHost();
        return;
    }
    // чтение ответа будет выполнено в onReadyRead когда данные придут
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
                    if (line.startsWith("Content-Length:")) {
                        contentLength = line.mid(15).trimmed().toInt();
                    }
                }
                headerParsed = true;
            } else {
                return;
            }
        }
        if (headerParsed && contentLength > 0 && buffer.size() >= contentLength) {
            QImage img;
            img.loadFromData(buffer.left(contentLength), "JPEG");
            lastImage = img;
            emit imageReceived(img);
            socket->disconnectFromHost();
            mode = None;
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
                int code = statusParts[1].trimmed().toInt();
                if (code >= 200 && code < 300) {
                    emit uploadFinished(true, "Upload successful");
                } else {
                    emit uploadFinished(false, QString("Server responded with code %1").arg(code));
                }
            } else {
                emit uploadFinished(false, "Invalid server response");
            }
        } else {
            emit uploadFinished(false, "Invalid server response");
        }
        socket->disconnectFromHost();
        mode = None;
    }
}

void JPEGClient::onError(QAbstractSocket::SocketError) {
    QString err = socket->errorString();
    if (mode == UploadImage) {
        emit uploadFinished(false, err);
    } else {
        emit errorOccurred(err);
    }
    mode = None;
}

QImage JPEGClient::getLastImage() const {
    return lastImage;
}

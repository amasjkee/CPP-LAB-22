#ifndef JPEGSERVER_H
#define JPEGSERVER_H

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QObject>
#include "jpegstrategy.h"

class JPEGServer : public QTcpServer {
    Q_OBJECT
public:
    explicit JPEGServer(QObject* parent = nullptr);
    void setStrategy(JPEGStrategy* strategy);
    void setImagePath(const QString& path);
protected:
    void incomingConnection(qintptr socketDescriptor) override;
private:
    JPEGStrategy* strategy;
    QString imagePath;
};

#endif // JPEGSERVER_H

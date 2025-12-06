#ifndef JPEGSERVER_SECURE_H
#define JPEGSERVER_SECURE_H

#include <QtNetwork/QSslServer>
#include <QtNetwork/QSslSocket>
#include <QObject>
#include "jpegstrategy.h"

class JPEGSslServer : public QSslServer {
    Q_OBJECT
public:
    explicit JPEGSslServer(QObject* parent = nullptr);
    void setStrategy(JPEGStrategy* strategy);
    void setImagePath(const QString& path);
protected:
    void incomingConnection(qintptr socketDescriptor) override;
private:
    JPEGStrategy* strategy;
    QString imagePath;
};

#endif // JPEGSERVER_SECURE_H

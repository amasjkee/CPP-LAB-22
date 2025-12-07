#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QtNetwork/QHostAddress>
#include "jpegserver_secure.h"
#include "jpegstrategy.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("JPEG Secure Server (SSL/GOST)");
    parser.addHelpOption();
    parser.addPositionalArgument("file", "Path to JPEG file to serve.");
    QCommandLineOption portOpt({"p", "port"}, "Port to listen on.", "port", "12345");
    QCommandLineOption progressiveOpt({"g", "progressive"}, "Serve as progressive JPEG.");
    parser.addOption(portOpt);
    parser.addOption(progressiveOpt);
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        qCritical() << "No JPEG file specified.";
        return 1;
    }
    QString filePath = args.first();
    int port = parser.value(portOpt).toInt();
    bool progressive = parser.isSet(progressiveOpt);

    JPEGSslServer server;
    server.setImagePath(filePath);
    if (progressive)
        server.setStrategy(new ProgressiveJPEGStrategy());
    else
        server.setStrategy(new StandardJPEGStrategy());
    
    if (!server.listen(QHostAddress::Any, port)) {
        qCritical() << "Secure server failed to start on port" << port;
        return 1;
    }
    
    qDebug() << "JPEG secure server started on port" << port << ", file:" << filePath << (progressive ? "(progressive)" : "(standard)");
    qDebug() << "Note: This is a demonstration of SSL/TLS encryption.";
    qDebug() << "For GOST encryption, additional configuration is required (see code comments).";
    
    return app.exec();
}


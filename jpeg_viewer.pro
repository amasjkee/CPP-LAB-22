QT += core gui widgets network

CONFIG += c++17

TARGET = jpeg_viewer
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    jpegclient.cpp \
    jpegclient_secure.cpp \
    jpegserver.cpp \
    jpegserver_secure.cpp \
    jpegloader.cpp \
    jpegsaver.cpp \
    imagehandler.cpp \
    jpegstrategy.cpp \
    servermanager.cpp

HEADERS += \
    mainwindow.h \
    jpegclient.h \
    jpegclient_secure.h \
    jpegserver.h \
    jpegserver_secure.h \
    jpegloader.h \
    jpegsaver.h \
    imagehandler.h \
    jpegstrategy.h \
    servermanager.h


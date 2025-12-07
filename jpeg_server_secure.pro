QT += core network

CONFIG += c++17 console

TARGET = jpeg_server_secure
TEMPLATE = app

SOURCES += \
    jpegserver_secure_main.cpp \
    jpegserver_secure.cpp \
    jpegstrategy.cpp

HEADERS += \
    jpegserver_secure.h \
    jpegstrategy.h


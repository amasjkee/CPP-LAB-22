QT += core network

CONFIG += c++17 console

TARGET = jpeg_server
TEMPLATE = app

SOURCES += \
    jpegserver_main.cpp \
    jpegserver.cpp \
    jpegstrategy.cpp

HEADERS += \
    jpegserver.h \
    jpegstrategy.h

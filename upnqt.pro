QT       += network

QT       -= gui

TARGET = upnqt
TEMPLATE = lib

DEFINES += UPNQT_LIBRARY

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    upnpconnection.cpp

HEADERS +=\
        upnqt_global.h \
    upnpconnection.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}

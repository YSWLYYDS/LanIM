QT += core gui widgets network sql concurrent
SOURCES += reportmodel.cpp reportpanel.cpp xlsxexporter.cpp
SOURCES += reportanalytics.cpp
HEADERS += reportanalytics.h businessstyle.h
HEADERS += reportmodel.h reportpanel.h xlsxexporter.h
SOURCES += ticketpanel.cpp
HEADERS += ticketpanel.h

TEMPLATE = app
TARGET = LanIMClient

CONFIG += c++17

SOURCES += \
        chatclient.cpp \
        main.cpp \
        mainwindow.cpp
HEADERS += \
        chatclient.h \
        mainwindow.h

FORMS += \
        mainwindow.ui

include(../common/common.pri)

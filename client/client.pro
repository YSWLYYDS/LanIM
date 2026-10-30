QT += core gui widgets network sql

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
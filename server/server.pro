QT += core network sql

TEMPLATE = app
TARGET = LanIMServer

CONFIG += console c++17
CONFIG -= app_bundle

SOURCES +=  \
    databasemanager.cpp \
    main.cpp \
    chatserver.cpp \
    messagerepository.cpp \
    sessionmanager.cpp \
    userrepository.cpp

HEADERS += \
    chatserver.h \
    databasemanager.h \
    messagerepository.h \
    sessionmanager.h \
    userrepository.h

include(../common/common.pri)


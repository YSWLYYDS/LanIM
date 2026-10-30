INCLUDEPATH += $$PWD
##$$PWD：当前这个 .pri 文件所在目录
HEADERS += \
    $$PWD/logging.h \
    $$PWD/protocol.h

SOURCES += \
    $$PWD/logging.cpp \
    $$PWD/protocol.cpp
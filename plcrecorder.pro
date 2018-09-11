#-------------------------------------------------
#
# Project created by QtCreator 2012-06-21T17:05:11
#
#-------------------------------------------------

QT       += core gui widgets

TARGET = plcrecorder
TEMPLATE = app


DEFINES += DAVE_LITTLE_ENDIAN


SOURCES += libnodave/nodave.c \
    main.cpp\
    mainwindow.cpp \
    plc.cpp \
    varmodel.cpp \
    global.cpp \
    outputdialog.cpp \
    specwidgets.cpp \
    timeoutsdialog.cpp

HEADERS  += mainwindow.h \
    libnodave/log2.h \
    libnodave/nodave.h \
    libnodave/openSocket.h \
    libnodave/setport.h \
    plc.h \
    varmodel.h \
    global.h \
    outputdialog.h \
    specwidgets.h \
    timeoutsdialog.h

FORMS    += mainwindow.ui \
    outputdialog.ui \
    timeoutsdialog.ui

RESOURCES += \
    plcrecorder.qrc

CONFIG += warn_on \
    link_pkgconfig \
    c++14

unix {
    DEFINES += LINUX
    SOURCES += libnodave/setport.c \
        libnodave/openSocket.c
}

win32 {
    DEFINES += BCCWIN
    SOURCES += libnodave/openSocketw.c \
        libnodave/setportw.c
    HEADERS += libnodave/openS7online.h
    LIBS += -lws2_32
}

RC_FILE = plcrecorder.rc

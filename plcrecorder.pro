#-------------------------------------------------
#
# Project created by QtCreator 2012-06-21T17:05:11
#
#-------------------------------------------------

QT       += core gui widgets printsupport

TARGET = plcrecorder
TEMPLATE = app


DEFINES += DAVE_LITTLE_ENDIAN


SOURCES += libnodave/nodave.c \
    main.cpp\
    mainwindow.cpp \
    plc.cpp \
    varmodel.cpp \
    global.cpp \
    specwidgets.cpp \
    qcustomplot-source/qcustomplot.cpp \
    graphform.cpp \
    settingsdialog.cpp

HEADERS  += mainwindow.h \
    libnodave/log2.h \
    libnodave/nodave.h \
    libnodave/openSocket.h \
    libnodave/setport.h \
    plc.h \
    varmodel.h \
    global.h \
    specwidgets.h \
    qcustomplot-source/qcustomplot.h \
    graphform.h \
    plc_p.h \
    settingsdialog.h

FORMS    += mainwindow.ui \
    graphform.ui \
    settingsdialog.ui

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

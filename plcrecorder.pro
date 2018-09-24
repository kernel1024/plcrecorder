#-------------------------------------------------
#
# Project created by QtCreator 2012-06-21T17:05:11
#
#-------------------------------------------------

QT       += core gui printsupport

TARGET = plcrecorder
TEMPLATE = app

greaterThan(QT_MAJOR_VERSION, 4) {
  QT += widgets
  DEFINES += HAVE_QT5
}

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
    settingsdialog.cpp \
    csvhandler.cpp

HEADERS  += mainwindow.h \
    libnodave/log2.h \
    libnodave/nodave.h \
    plc.h \
    varmodel.h \
    global.h \
    specwidgets.h \
    qcustomplot-source/qcustomplot.h \
    graphform.h \
    plc_p.h \
    settingsdialog.h \
    csvhandler.h

FORMS    += mainwindow.ui \
    graphform.ui \
    settingsdialog.ui

RESOURCES += \
    plcrecorder.qrc

CONFIG += warn_on \
    link_pkgconfig \

unix {
    DEFINES += LINUX
    SOURCES += libnodave/setport.c \
        libnodave/openSocket.c
    HEADERS += libnodave/openSocket.h \
        libnodave/setport.h \
}

win32 {
    DEFINES += BCCWIN DOEXPORT
    LIBS += -lws2_32
    SOURCES += libnodave/openSocketw.c \
        libnodave/setportw.c
    HEADERS += libnodave/openS7online.h
}

RC_FILE = plcrecorder.rc

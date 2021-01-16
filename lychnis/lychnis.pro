QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = lychnis
TEMPLATE = app

RC_FILE = app.rc

DESTDIR = ../bin

DEFINES += QT_DEPRECATED_WARNINGS

QMAKE_LFLAGS_RELEASE    += /MAP
QMAKE_CXXFLAGS_RELEASE    += /Zi
QMAKE_LFLAGS_RELEASE    += /debug /opt:ref

include(dependencies.pri)

SOURCES += main.cpp\
    mainwindow.cpp \
    contrastadjuster.cpp \
    rangeslider.cpp \
    menubar.cpp \
    common.cpp \
    stackslider.cpp \
    viewerpanel.cpp \
    volumeviewer.cpp \
    controlpanel.cpp \
    widgets.cpp \
    statusbar.cpp \
    vtk/vtkVolumePicker2.cxx \
    util.cpp \
    imagewriter.cpp \
    worldviewer.cpp

HEADERS  += mainwindow.h \
    contrastadjuster.h \
    rangeslider.h \
    menubar.h \
    common.h \
    stackslider.h \
    viewerpanel.h \
    volumeviewer.h \
    controlpanel.h \
    widgets.h \
    statusbar.h \
    vtk/vtkVolumePicker2.h \
    util.h \
    imagewriter.h \
    worldviewer.h

RESOURCES += \
    assets.qrc

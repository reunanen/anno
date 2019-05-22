#-------------------------------------------------
#
# Project created by QtCreator 2014-10-21T13:37:31
#
#-------------------------------------------------

QT       += core gui uitools

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = anno
TEMPLATE = app

win32 {
    LIBS += Shell32.lib
}

SOURCES += main.cpp \
    mainwindow.cpp \
    QResultImageView/QResultImageView.cpp \
    QResultImageView/qt-image-flood-fill/qfloodfill.cpp \
    cpp-move-file-to-trash/move-file-to-trash.cpp

HEADERS  += mainwindow.h \
    QResultImageView/QResultImageView.h \
    QResultImageView/qt-image-flood-fill/qfloodfill.h \
    version.h

FORMS    += mainwindow.ui \
    about.ui

RC_FILE = anno.rc

RESOURCES += \
    anno.qrc

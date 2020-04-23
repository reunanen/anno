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

LIBS += ../librabbitmq/release/librabbitmq.lib

INCLUDEPATH += ../Numcore_messaging_library
INCLUDEPATH += ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq
INCLUDEPATH += ../Numcore_messaging_library/messaging/numrabw/amqpcpp/include

DEFINES += AMQP_STATIC
DEFINES += AMQP_NO_SSL
DEFINES += GUID_WINDOWS
DEFINES += snprintf=_snprintf

SOURCES += ../main.cpp \
    ../mainwindow.cpp \
    ../QResultImageView/QResultImageView.cpp \
    ../QResultImageView/qt-image-flood-fill/qfloodfill.cpp \
    ../cpp-move-file-to-trash/move-file-to-trash.cpp \
    ../Numcore_messaging_library/messaging/claim/AttributeMessage.cpp \
    ../Numcore_messaging_library/messaging/claim/MessageStreaming.cpp \
    ../Numcore_messaging_library/messaging/claim/PostOffice.cpp \
    ../Numcore_messaging_library/messaging/claim/PostOfficeInitializer.cpp \
    ../Numcore_messaging_library/messaging/slaim/messaging.cpp \
    ../Numcore_messaging_library/numcfc/IdGenerator.cpp \
    ../Numcore_messaging_library/numcfc/IniFile.cpp \
    ../Numcore_messaging_library/numcfc/Logger.cpp \
    ../Numcore_messaging_library/numcfc/ThreadRunner.cpp \
    ../Numcore_messaging_library/numcfc/Time.cpp \
    ../Numcore_messaging_library/messaging/numrabw/numrabw_postoffice.cpp \
    ../Numcore_messaging_library/messaging/numrabw/amqpcpp/src/AMQP.cpp \
    ../Numcore_messaging_library/messaging/numrabw/amqpcpp/src/AMQPBase.cpp \
    ../Numcore_messaging_library/messaging/numrabw/amqpcpp/src/AMQPException.cpp \
    ../Numcore_messaging_library/messaging/numrabw/amqpcpp/src/AMQPExchange.cpp \
    ../Numcore_messaging_library/messaging/numrabw/amqpcpp/src/AMQPMessage.cpp \
    ../Numcore_messaging_library/messaging/numrabw/amqpcpp/src/AMQPQueue.cpp \
    ../Numcore_messaging_library/messaging/numrabw/crossguid/Guid.cpp

HEADERS  += ../mainwindow.h \
    ../QResultImageView/QResultImageView.h \
    ../QResultImageView/qt-image-flood-fill/qfloodfill.h \
    ../version.h \
    ../Numcore_messaging_library/messaging/claim/AttributeMessage.h \
    ../Numcore_messaging_library/messaging/claim/MessageStreaming.h \
    ../Numcore_messaging_library/messaging/claim/PostOffice.h \
    ../Numcore_messaging_library/messaging/claim/PostOfficeInitializer.h \
    ../Numcore_messaging_library/messaging/claim/ThroughputStatistics.h \
    ../Numcore_messaging_library/messaging/slaim/buffer.h \
    ../Numcore_messaging_library/messaging/slaim/bufferitem.h \
    ../Numcore_messaging_library/messaging/slaim/errorlog.h \
    ../Numcore_messaging_library/messaging/slaim/message.h \
    ../Numcore_messaging_library/messaging/slaim/postoffice.h \
    ../Numcore_messaging_library/numcfc/IdGenerator.h \
    ../Numcore_messaging_library/numcfc/IniFile.h \
    ../Numcore_messaging_library/numcfc/Logger.h \
    ../Numcore_messaging_library/numcfc/StringBuilder.h \
    ../Numcore_messaging_library/numcfc/ThreadRunner.h \
    ../Numcore_messaging_library/numcfc/Time.h \
    ../Numcore_messaging_library/messaging/numrabw/LimitedSizeBuffer.h \
    ../Numcore_messaging_library/messaging/numrabw/numrabw_postoffice.h \
    ../Numcore_messaging_library/messaging/numrabw/shared_buffer/shared_buffer.h \
    ../Numcore_messaging_library/messaging/numrabw/amqpcpp/include/AMQPcpp.h \
    ../Numcore_messaging_library/messaging/numrabw/crossguid/Guid.hpp

FORMS    += ../mainwindow.ui \
    ../about.ui

RC_FILE = ../anno.rc

RESOURCES += \
    ../anno.qrc

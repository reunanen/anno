TEMPLATE = lib

CONFIG -= qt
CONFIG += staticlib

INCLUDEPATH += ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq

DEFINES += AMQP_STATIC
DEFINES += AMQP_NO_SSL
DEFINES += AMQ_PLATFORM=\\\"Windows\\\"
DEFINES += HAVE_SELECT
DEFINES += inline=__inline

win32 {
    LIBS += Shell32.lib
    LIBS += Ws2_32.lib
}

SOURCES += \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_api.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_connection.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_consumer.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_framing.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_hostcheck.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_mem.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_socket.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_table.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_tcp_socket.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_time.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_url.c \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/win32/threads.c

HEADERS += \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp.h \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_framing.h \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_hostcheck.h \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_private.h \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_socket.h \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_table.h \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_tcp_socket.h \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/amqp_time.h \
    ../Numcore_messaging_library/messaging/numrabw/rabbitmq-c/librabbitmq/win32/threads.h \

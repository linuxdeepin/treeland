TARGET = waylibserver
TEMPLATE = lib
QT += gui gui-private quick quick-private egl_support-private input_support-private
QT.egl_support_private.uses -= libdl

DEFINES += LIBWAYLIB_SERVER_LIBRARY

HEADERS += \
    $$PWD/wglobal.h

SOURCES += \
    $$PWD/wglobal.cpp

include(kernel/kernel.pri)
include(qtquick/qtquick.pri)
include(utils/utils.pri)

#TODO: Don't assumed the prefix is /usr
includes.path = /usr/include
includes.files += \
    $$PWD/wglobal.h

target.path = $$LIB_INSTALL_DIR

INSTALLS += includes target

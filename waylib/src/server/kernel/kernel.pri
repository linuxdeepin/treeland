CONFIG += link_pkgconfig
PKGCONFIG += wlroots wayland-server xkbcommon egl
DEFINES += WLR_USE_UNSTABLE
INCLUDEPATH += $$PWD

WAYLAND_PROTOCOLS=$$system(pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$$system(pkg-config --variable=wayland_scanner wayland-scanner)

system($$WAYLAND_SCANNER server-header $$WAYLAND_PROTOCOLS/stable/xdg-shell/xdg-shell.xml $$OUT_PWD/xdg-shell-protocol.h)
system($$WAYLAND_SCANNER public-code $$WAYLAND_PROTOCOLS/stable/xdg-shell/xdg-shell.xml $$OUT_PWD/xdg-shell-protocol.c)

HEADERS += \
    $$PWD/wbackend.h \
    $$PWD/wcursor.h \
    $$PWD/winputdevice.h \
    $$PWD/woutput.h \
    $$PWD/woutputlayout.h \
    $$PWD/wseat.h \
    $$PWD/wserver.h \
    $$PWD/wsurface.h \
    $$PWD/wsurfacelayout.h \
    $$PWD/wtexture.h \
    $$PWD/wtypes.h \
    $$PWD/wxcursormanager.h \
    $$PWD/wxdgshell.h \
    $$PWD/wxdgsurface.h

SOURCES += \
    $$PWD/wbackend.cpp \
    $$PWD/wcursor.cpp \
    $$PWD/winputdevice.cpp \
    $$PWD/woutput.cpp \
    $$PWD/woutputlayout.cpp \
    $$PWD/wseat.cpp \
    $$PWD/wserver.cpp \
    $$PWD/wsurface.cpp \
    $$PWD/wsurfacelayout.cpp \
    $$PWD/wtexture.cpp \
    $$PWD/wtypes.cpp \
    $$PWD/wxcursormanager.cpp \
    $$PWD/wxdgshell.cpp \
    $$PWD/wxdgsurface.cpp

includes.files += \
    $$PWD/wbackend.h \
    $$PWD/wcursor.h \
    $$PWD/winputdevice.h \
    $$PWD/woutput.h \
    $$PWD/woutputlayout.h \
    $$PWD/wseat.h \
    $$PWD/wserver.h \
    $$PWD/wsurface.h \
    $$PWD/wsurfacelayout.h \
    $$PWD/wtexture.h \
    $$PWD/wtypes.h \
    $$PWD/wxcursormanager.h \
    $$PWD/wxdgshell.h \
    $$PWD/wxdgsurface.h \
    \
    $$PWD/WOutput \
    $$PWD/WServer \
    $$PWD/WServerInterface \
    $$PWD/WBackend \
    $$PWD/WCursor \
    $$PWD/WInputDevice \
    $$PWD/WOutputLayout \
    $$PWD/WSeat \
    $$PWD/WInputEvent \
    $$PWD/WTexture \
    $$PWD/WXCursorManager \
    $$PWD/WXdgShell \
    $$PWD/WXdgSurface \
    $$PWD/WSurfaceLayout

include($$PWD/private/private.pri)

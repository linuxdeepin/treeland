set(TARGET waylibserver)

find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
    Core
    Gui
    Quick
    EglSupport
    InputSupport
    REQUIRED
)
find_package(PkgConfig)

pkg_search_module(WLROOTS wlroots>=0.14)
pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

execute_process(COMMAND ${WAYLAND_SCANNER}
    server-header
    ${WAYLAND_PROTOCOLS}/stable/xdg-shell/xdg-shell.xml
    ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.h
)

execute_process(COMMAND ${WAYLAND_SCANNER}
    public-code
    ${WAYLAND_PROTOCOLS}/stable/xdg-shell/xdg-shell.xml
    ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
)

add_definitions(-DWLR_USE_UNSTABLE -DLIBWAYLIB_SERVER_LIBRARY)

set(GLOBAL_SOURCES
    wglobal.cpp
)

set(KERNEL_SOURCES
    kernel/wbackend.cpp
    kernel/wcursor.cpp
    kernel/winputdevice.cpp
    kernel/woutput.cpp
    kernel/woutputlayout.cpp
    kernel/wseat.cpp
    kernel/wserver.cpp
    kernel/wsurface.cpp
    kernel/wsurfacelayout.cpp
    kernel/wtexture.cpp
    kernel/wtypes.cpp
    kernel/wxcursormanager.cpp
    kernel/wxdgshell.cpp
    kernel/wxdgsurface.cpp
)

set(QTQUICK_SOURCES
    qtquick/wsurfaceitem.cpp
    qtquick/wsurfacemanager.cpp
)

set(UTILS_SOURCES
    utils/wsignalconnector.cpp
    utils/wtools.cpp
)

add_library(${TARGET}
    SHARED
    ${GLOBAL_SOURCES}
    ${KERNEL_SOURCES}
    ${QTQUICK_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(${TARGET}
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::EglSupport
    Qt${QT_VERSION_MAJOR}::InputSupport
    ${WLROOTS_LIBRARIES}
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

target_include_directories(${TARGET}
    PRIVATE
    ${Qt${QT_VERSION_MAJOR}Gui_PRIVATE_INCLUDE_DIRS}
    ${Qt${QT_VERSION_MAJOR}Quick_PRIVATE_INCLUDE_DIRS}
    ${Qt${QT_VERSION_MAJOR}EglSupport_PRIVATE_INCLUDE_DIRS}
    ${Qt${QT_VERSION_MAJOR}InputSupport_PRIVATE_INCLUDE_DIRS}
    ${WLROOTS_INCLUDE_DIRS}
)

target_include_directories(${TARGET}
    PUBLIC
    kernel
    qtquick
    utils
)

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

include(${PROJECT_SOURCE_DIR}/cmake/WaylandScannerHelpers.cmake)
ws_generate(server stable/xdg-shell/xdg-shell.xml xdg-shell-protocol)

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

set(HEADERS
    wglobal.h

    kernel/wbackend.h
    kernel/wcursor.h
    kernel/winputdevice.h
    kernel/woutput.h
    kernel/woutputlayout.h
    kernel/wseat.h
    kernel/wserver.h
    kernel/wsurface.h
    kernel/wsurfacelayout.h
    kernel/wtexture.h
    kernel/wtypes.h
    kernel/wxcursormanager.h
    kernel/wxdgshell.h
    kernel/wxdgsurface.h
    kernel/WOutput
    kernel/WServer
    kernel/WServerInterface
    kernel/WBackend
    kernel/WCursor
    kernel/WInputDevice
    kernel/WOutputLayout
    kernel/WSeat
    kernel/WInputEvent
    kernel/WTexture
    kernel/WXCursorManager
    kernel/WXdgShell
    kernel/WXdgSurface
    kernel/WSurfaceLayout

    qtquick/wquickrendercontrol.h
    qtquick/wsurfaceitem.h
    qtquick/wsurfacemanager.h
    qtquick/WQuickRenderControl
    qtquick/WSurfaceItem
    qtquick/WSurfaceManager

    utils/wsignalconnector.h
    utils/wtools.h
    utils/wthreadutils.h
    utils/WSignalConnector
    utils/WThreadUtils
)

add_library(${TARGET}
    SHARED
    ${GLOBAL_SOURCES}
    ${KERNEL_SOURCES}
    ${QTQUICK_SOURCES}
    ${UTILS_SOURCES}
)

set_target_properties(${TARGET}
    PROPERTIES
        VERSION ${CMAKE_PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${HEADERS}"
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

install(TARGETS ${TARGET}
    LIBRARY
    PUBLIC_HEADER
        COMPONENT Development
        DESTINATION include/${TARGET}
)

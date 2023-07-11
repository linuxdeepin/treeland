set(TARGET waylibserver)

find_package(Qt6
    COMPONENTS
    Core
    Gui
    Quick
    REQUIRED
)

find_package(PkgConfig)
pkg_search_module(PIXMAN REQUIRED pixman-1)
pkg_search_module(LIBDRM REQUIRED IMPORTED_TARGET libdrm)

include(${PROJECT_SOURCE_DIR}/cmake/WaylandScannerHelpers.cmake)
ws_generate(server stable/xdg-shell/xdg-shell.xml xdg-shell-protocol)

add_definitions(-DWLR_USE_UNSTABLE -DLIBWAYLIB_SERVER_LIBRARY)

set(KERNEL_SOURCES
    kernel/wbackend.cpp
    kernel/wcursor.cpp
    kernel/winputdevice.cpp
    kernel/woutput.cpp
    kernel/wseat.cpp
    kernel/wserver.cpp
    kernel/wsurface.cpp
    kernel/wsurfacehandler.cpp
    kernel/wtexture.cpp
    kernel/wtypes.cpp
    kernel/wxcursormanager.cpp
    kernel/wxdgshell.cpp
    kernel/wxdgsurface.cpp
    kernel/woutputlayout.cpp
    kernel/wxcursorimage.cpp
    kernel/wglobal.cpp
)

# TODO: Using wlr_damage_ring instead of wlr_output_damage
option(WAYLIB_DISABLE_OUTPUT_DAMAGE "Don't use the damage manager for outputs" ON)

if (WAYLIB_DISABLE_OUTPUT_DAMAGE)
    add_definitions(-DWAYLIB_DISABLE_OUTPUT_DAMAGE)
else()
    list(APPEND KERNEL_SOURCES kernel/woutputdamage.cpp)
endif()

set(QTQUICK_SOURCES
    qtquick/wsurfaceitem.cpp
    qtquick/woutputhelper.cpp
    qtquick/wquickwaylandserver.cpp
    qtquick/woutputrenderwindow.cpp
    qtquick/woutputviewport.cpp
    qtquick/woutputlayoutitem.cpp
    qtquick/wquickoutputlayout.cpp
    qtquick/wquickcursor.cpp

    qtquick/private/wquickxdgshell.cpp
    qtquick/private/wquickbackend.cpp
    qtquick/private/wquickseat.cpp
    qtquick/private/wwaylandcompositor.cpp
)

set(UTILS_SOURCES
    utils/wsignalconnector.cpp
    utils/wtools.cpp
    utils/wthreadutils.cpp
    utils/wimagebuffer.cpp
)

set(QPA_SOURCES
    platformplugin/qwlrootsintegration.cpp
    platformplugin/qwlrootscreen.cpp
    platformplugin/qwlrootswindow.cpp
    platformplugin/qwlrootscursor.cpp
)

set(HEADERS
    kernel/wglobal.h
    kernel/wbackend.h
    kernel/wcursor.h
    kernel/winputdevice.h
    kernel/woutput.h
    kernel/wseat.h
    kernel/wserver.h
    kernel/wsurface.h
    kernel/wsurfacehandler.h
    kernel/wtexture.h
    kernel/wtypes.h
    kernel/wxcursormanager.h
    kernel/wxdgshell.h
    kernel/wxdgsurface.h
    kernel/woutputlayout.h
    kernel/wxcursorimage.h
    kernel/WOutput
    kernel/WServer
    kernel/WServerInterface
    kernel/WBackend
    kernel/WCursor
    kernel/WInputDevice
    kernel/WSeat
    kernel/WInputEvent
    kernel/WTexture
    kernel/WXCursorManager
    kernel/WXdgShell
    kernel/WXdgSurface

    qtquick/wsurfaceitem.h
    qtquick/WSurfaceItem
    qtquick/woutputhelper.h
    qtquick/woutputlayoutitem.h
    qtquick/wquickoutputlayout.h
    qtquick/wquickwaylandserver.h
    qtquick/woutputrenderwindow.h
    qtquick/woutputviewport.h
    qtquick/wquickcursor.h

    utils/wsignalconnector.h
    utils/wtools.h
    utils/wthreadutils.h
    utils/WSignalConnector
    utils/WThreadUtils
    utils/wimagebuffer.h
)

set(PRIVATE_HEADERS
    platformplugin/types.h
)

add_library(${TARGET}
    SHARED
    ${KERNEL_SOURCES}
    ${QTQUICK_SOURCES}
    ${UTILS_SOURCES}
    ${QPA_SOURCES}
    ${HEADERS}
    ${PRIVATE_HEADERS}
)

qt_add_qml_module(${TARGET}
    URI Waylib.Server
    VERSION "${PROJECT_VERSION}"
)

set_target_properties(${TARGET}
    PROPERTIES
        VERSION ${CMAKE_PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${HEADERS}"
        PRIVATE_HEADER "${PRIVATE_HEADERS}"
)

set(QT_LIBRAIES
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

if(WITH_SUBMODULE_QWLROOTS)
    target_link_libraries(${TARGET}
        PUBLIC
        qwlroots
    )
else()
    find_package(QWlroots)
    target_link_libraries(${TARGET}
        PUBLIC
        QWlroots::QWlroots
    )
endif()

target_link_libraries(${TARGET}
    PRIVATE
    ${QT_LIBRAIES}
    ${WLROOTS_LIBRARIES}
    PkgConfig::LIBDRM
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

target_include_directories(${TARGET}
    PRIVATE
    ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
    ${Qt6Quick_PRIVATE_INCLUDE_DIRS}
    ${Qt6EglSupport_PRIVATE_INCLUDE_DIRS}
    ${Qt6InputSupport_PRIVATE_INCLUDE_DIRS}
    ${WLROOTS_INCLUDE_DIRS}
)

target_include_directories(${TARGET}
    PUBLIC
    kernel
    qtquick
    qtquick/private
    utils
)

install(TARGETS ${TARGET}
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER
        COMPONENT Development
        DESTINATION include/${TARGET}
)

include(${PROJECT_SOURCE_DIR}/cmake/Helpers.cmake)
add_cmake_module(WaylibServer ${TARGET} ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${TARGET})
add_pkgconfig_module(${TARGET} ${TARGET} ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${TARGET}
    "qwlroots, Qt6Gui, Qt6Quick"
)

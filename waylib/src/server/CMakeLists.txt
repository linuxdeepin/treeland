set(TARGET waylibserver)

find_package(Qt6
    COMPONENTS
    Core
    Gui
    Quick
    REQUIRED
)
qt_standard_project_setup(REQUIRES 6.4)

if(QT_KNOWN_POLICY_QTP0001) # this policy was introduced in Qt 6.5
    qt_policy(SET QTP0001 NEW)
    # the RESOURCE_PREFIX argument for qt_add_qml_module() defaults to ":/qt/qml/"
endif()

find_package(PkgConfig)
pkg_search_module(PIXMAN REQUIRED pixman-1)
pkg_search_module(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

include(${PROJECT_SOURCE_DIR}/cmake/WaylandScannerHelpers.cmake)
ws_generate(
    server
    wayland-protocols
    stable/xdg-shell/xdg-shell.xml
    xdg-shell-protocol
)
ws_generate(
    server
    wayland-protocols
    staging/cursor-shape/cursor-shape-v1.xml
    cursor-shape-v1-protocol
)

add_definitions(-DWLR_USE_UNSTABLE -DLIBWAYLIB_SERVER_LIBRARY)

set(KERNEL_SOURCES
    kernel/wbackend.cpp
    kernel/wcursor.cpp
    kernel/winputdevice.cpp
    kernel/woutput.cpp
    kernel/wseat.cpp
    kernel/wserver.cpp
    kernel/wsurface.cpp

    kernel/wtexture.cpp
    kernel/wtypes.cpp

    kernel/wxdgshell.cpp
    kernel/wxdgsurface.cpp
    kernel/woutputlayout.cpp
    kernel/wxcursorimage.cpp
    kernel/wglobal.cpp
    kernel/wsocket.cpp
)

set(QTQUICK_SOURCES
    qtquick/wsurfaceitem.cpp
    qtquick/woutputhelper.cpp
    qtquick/wquickwaylandserver.cpp
    qtquick/woutputrenderwindow.cpp
    qtquick/woutputviewport.cpp
    qtquick/woutputitem.cpp
    qtquick/woutputlayoutitem.cpp
    qtquick/wquickoutputlayout.cpp
    qtquick/wquickcursor.cpp
    qtquick/wquickobserver.cpp
    qtquick/weventjunkman.cpp
    qtquick/wrenderhelper.cpp
    qtquick/wquicktextureproxy.cpp

    qtquick/private/wquickxdgshell.cpp
    qtquick/private/wquickbackend.cpp
    qtquick/private/wquickseat.cpp
    qtquick/private/wwaylandcompositor.cpp
    qtquick/private/wquickcoordmapper.cpp
    qtquick/private/wquicksocket.cpp
    qtquick/private/wqmldynamiccreator.cpp
    qtquick/private/wqmlhelper.cpp
    qtquick/private/wquickxdgdecorationmanager.cpp
    qtquick/private/wquickcursorshape.cpp
)

set(UTILS_SOURCES
    utils/wtools.cpp
    utils/wthreadutils.cpp
    utils/wimagebuffer.cpp
)

set(QPA_SOURCES
    platformplugin/qwlrootsintegration.cpp
    platformplugin/qwlrootscreen.cpp
    platformplugin/qwlrootswindow.cpp
    platformplugin/qwlrootscursor.cpp
    platformplugin/types.cpp
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
    kernel/wtexture.h
    kernel/wtypes.h
    kernel/wxdgshell.h
    kernel/wxdgsurface.h
    kernel/woutputlayout.h
    kernel/wxcursorimage.h
    kernel/wsocket.h
    kernel/wtoplevelsurface.h

    kernel/WOutput
    kernel/WServer
    kernel/WServerInterface
    kernel/WBackend
    kernel/WCursor
    kernel/WInputDevice
    kernel/WSeat
    kernel/WInputEvent
    kernel/WTexture
    kernel/WXdgShell
    kernel/WXdgSurface
    kernel/WSurface

    qtquick/wsurfaceitem.h
    qtquick/WSurfaceItem
    qtquick/woutputhelper.h
    qtquick/woutputlayoutitem.h
    qtquick/wquickoutputlayout.h
    qtquick/wquickwaylandserver.h
    qtquick/woutputrenderwindow.h
    qtquick/woutputviewport.h
    qtquick/woutputitem.h
    qtquick/wquickcursor.h
    qtquick/wquickobserver.h
    qtquick/weventjunkman.h
    qtquick/wrenderhelper.h
    qtquick/wquicktextureproxy.h

    utils/wtools.h
    utils/wthreadutils.h
    utils/WThreadUtils
    utils/wimagebuffer.h
)

set(PRIVATE_HEADERS
    platformplugin/types.h
    kernel/private/wsurface_p.h
    qtquick/private/woutputviewport_p.h
    qtquick/private/wquickcoordmapper_p.h
    qtquick/private/woutputitem_p.h
    qtquick/private/wquicksocket_p.h
    qtquick/private/wqmldynamiccreator_p.h
    qtquick/private/wqmlhelper_p.h
    qtquick/private/wquickxdgdecorationmanager_p.h
    qtquick/private/wquickcursorshape_p.h
    qtquick/private/wquicktextureproxy_p.h
)

if(NOT DISABLE_XWAYLAND)
    list(APPEND KERNEL_SOURCES
        kernel/wxwayland.cpp
        kernel/wxwaylandsurface.cpp
    )

    list(APPEND QTQUICK_SOURCES
        qtquick/private/wquickxwayland.cpp
    )

    list(APPEND HEADERS
        kernel/wxwayland.h
        kernel/wxwaylandsurface.h
    )

    list(APPEND PRIVATE_HEADERS
        qtquick/private/wquickxwayland_p.h
    )
endif()

add_library(${TARGET}
    SHARED
    ${KERNEL_SOURCES}
    ${UTILS_SOURCES}
    ${QPA_SOURCES}
    ${QTQUICK_SOURCES}
)

qt_add_qml_module(${TARGET}
    URI Waylib.Server
    VERSION "1.0" # Qt bug: The major version must be larger than 0
    OUTPUT_DIRECTORY "Waylib/Server"
    SOURCES
        ${HEADERS}
        ${PRIVATE_HEADERS}
)

set_target_properties(${TARGET}
    PROPERTIES
        VERSION ${CMAKE_PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${HEADERS}"
	# PRIVATE_HEADER "${PRIVATE_HEADERS}"
	# CMake Warning has PRIVATE_HEADER files but no PRIVATE_HEADER DESTINATION
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
    PkgConfig::WLROOTS
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

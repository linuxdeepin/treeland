set(TARGET waylibserver)

set(WAYLIB_CMAKE_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/WaylibServer"
    CACHE STRING "Install directory for waylib cmake files"
)
set(WAYLIB_INCLUDE_INSTALL_DIR
    "${CMAKE_INSTALL_INCLUDEDIR}/${TARGET}"
    CACHE STRING "Install directory for waylib headers"
)

set(QT_COMPONENTS Core Gui Quick)
find_package(Qt6 COMPONENTS ${QT_COMPONENTS} REQUIRED)

qt_standard_project_setup(REQUIRES 6.6)

if(QT_KNOWN_POLICY_QTP0001) # this policy was introduced in Qt 6.5
    qt_policy(SET QTP0001 NEW)
    # the RESOURCE_PREFIX argument for qt_add_qml_module() defaults to ":/qt/qml/"
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.19)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(WAYLAND_PROTOCOLS REQUIRED IMPORTED_TARGET wayland-protocols)
pkg_search_module(WLR_PROTOCOLS REQUIRED wlr-protocols)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_search_module(XCB REQUIRED IMPORTED_TARGET xcb)
pkg_search_module(EGL REQUIRED IMPORTED_TARGET egl)

add_compile_definitions(WLR_PRIVATE=)

ws_generate(
    server
    wayland-protocols
    unstable/text-input/text-input-unstable-v1.xml
    text-input-unstable-v1-protocol
)

ws_generate(
    server
    wayland-protocols
    ${CMAKE_CURRENT_LIST_DIR}/protocols/private/text-input-unstable-v2.xml
    text-input-unstable-v2-protocol
)

ws_generate(
    server
    wayland-protocols
    unstable/xdg-output/xdg-output-unstable-v1.xml
    xdg-output-unstable-v1-protocol
)

ws_generate(
    server
    wayland-protocols
    staging/color-management/color-management-v1.xml
    color-management-v1-protocol
)

#TODO: Remove after wlroots 0.20
ws_generate(
    server
    wayland-protocols
    staging/ext-image-capture-source/ext-image-capture-source-v1.xml
    ext-image-capture-source-v1-protocol
)

#TODO: Remove after wlroots 0.20
ws_generate(
    server
    wayland-protocols
    staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml
    ext-foreign-toplevel-list-v1-protocol
)

set(SOURCES
    kernel/wbackend.cpp
    kernel/wcursor.cpp
    kernel/winputdevice.cpp
    kernel/woutput.cpp
    kernel/wseat.cpp
    kernel/wevent.cpp
    kernel/wserver.cpp
    kernel/wsurface.cpp
    kernel/wtoplevelsurface.cpp


    kernel/wtypes.cpp

    kernel/woutputlayout.cpp
    kernel/wxcursorimage.cpp
    kernel/wglobal.cpp
    kernel/wsocket.cpp

    qtquick/wsurfaceitem.cpp
    qtquick/woutputhelper.cpp
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
    qtquick/woutputlayer.cpp
    qtquick/wrenderbufferblitter.cpp
    qtquick/wxdgtoplevelsurfaceitem.cpp
    qtquick/wxdgpopupsurfaceitem.cpp
    qtquick/wlayersurfaceitem.cpp
    qtquick/wxwaylandsurfaceitem.cpp
    qtquick/wqmlcreator.cpp
    qtquick/winputpopupsurfaceitem.cpp
    qtquick/wsgtextureprovider.cpp
    qtquick/wtextureproviderprovider.cpp

    qtquick/private/wquickcoordmapper.cpp
    qtquick/private/wquicksocketattached.cpp
    qtquick/private/wqmlhelper.cpp
    qtquick/private/wbufferrenderer.cpp
    qtquick/private/wrenderbuffernode.cpp

    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/text-input-unstable-v1-protocol.c
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/text-input-unstable-v2-protocol.c
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/xdg-output-unstable-v1-protocol.c
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/ext-image-capture-source-v1-protocol.c #TODO: Remove after wlroots 0.20
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/ext-foreign-toplevel-list-v1-protocol.c #TODO Remove after wlroots 0.20

    utils/wtools.cpp
    utils/wthreadutils.cpp
    utils/wimagebuffer.cpp
    utils/wcursorimage.cpp
    utils/wextimagecapturesourcev1impl.cpp
    utils/wbufferdumper.cpp

    platformplugin/qwlrootsintegration.cpp
    platformplugin/qwlrootscreen.cpp
    platformplugin/qwlrootswindow.cpp
    platformplugin/qwlrootscursor.cpp
    platformplugin/types.cpp

    protocols/wxdgshell.cpp
    protocols/wxdgsurface.cpp
    protocols/wxdgtoplevelsurface.cpp
    protocols/wxdgpopupsurface.cpp

    protocols/wxwayland.cpp
    protocols/wxwaylandsurface.cpp
    protocols/wlayersurface.cpp
    protocols/wforeigntoplevelv1.cpp
    protocols/wxdgoutput.cpp
    protocols/wxdgdecorationmanager.cpp
    protocols/wlayershell.cpp
    protocols/winputmethodhelper.cpp
    protocols/winputpopupsurface.cpp
    protocols/private/winputmethodv2.cpp
    protocols/private/wtextinputv1.cpp
    protocols/private/wtextinputv2.cpp
    protocols/private/wtextinputv3.cpp
    protocols/private/wvirtualkeyboardv1.cpp
    protocols/ext_foreign_toplevel_image_capture_source.c #TODO: Remove after wlroots 0.20
    protocols/wcursorshapemanagerv1.cpp
    protocols/woutputmanagerv1.cpp
    protocols/wextforeigntoplevellistv1.cpp
)

set(HEADERS
    kernel/wglobal.h
    kernel/wbackend.h
    kernel/wcursor.h
    kernel/winputdevice.h
    kernel/woutput.h
    kernel/wseat.h
    kernel/wevent.h
    kernel/wserver.h
    kernel/wsurface.h
    kernel/wtypes.h
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
    kernel/WEvent
    kernel/WInputEvent
    kernel/WSurface

    qtquick/wsurfaceitem.h
    qtquick/WSurfaceItem
    qtquick/woutputhelper.h
    qtquick/woutputlayoutitem.h
    qtquick/wquickoutputlayout.h
    qtquick/woutputrenderwindow.h
    qtquick/woutputviewport.h
    qtquick/woutputitem.h
    qtquick/wquickcursor.h
    qtquick/wquickobserver.h
    qtquick/weventjunkman.h
    qtquick/wrenderhelper.h
    qtquick/wquicktextureproxy.h
    qtquick/woutputlayer.h
    qtquick/wrenderbufferblitter.h
    qtquick/wxdgtoplevelsurfaceitem.h
    qtquick/wxdgpopupsurfaceitem.h
    qtquick/wlayersurfaceitem.h
    qtquick/wxwaylandsurfaceitem.h
    qtquick/winputpopupsurfaceitem.h
    qtquick/wqmlcreator.h
    qtquick/wsgtextureprovider.h
    qtquick/wtextureproviderprovider.h

    utils/wtools.h
    utils/wthreadutils.h
    utils/WThreadUtils
    utils/wimagebuffer.h
    utils/wcursorimage.h
    utils/WCursorImage
    utils/wwrappointer.h
    utils/WWrapPointer
    utils/wextimagecapturesourcev1impl.h
    utils/wbufferdumper.h
    utils/WBufferDumper

    protocols/wxdgshell.h
    protocols/WXdgShell
    protocols/wxdgsurface.h
    protocols/WXdgSurface
    protocols/wxdgtoplevelsurface.h
    protocols/wxdgpopupsurface.h
    protocols/wlayersurface.h
    protocols/wxdgdecorationmanager.h
    protocols/WXdgDecorationManager
    protocols/WLayerSurface
    protocols/wforeigntoplevelv1.h
    protocols/WForeignToplevel
    protocols/wxdgoutput.h
    protocols/WXdgOutput
    protocols/WInputMethodHelper
    protocols/winputmethodhelper.h
    protocols/winputpopupsurface.h
    protocols/WInputPopupSurface
    protocols/wcursorshapemanagerv1.h
    protocols/WCursorShapeManagerV1
    protocols/woutputmanagerv1.h
    protocols/WOutputManagerV1
    protocols/wlayershell.h
    protocols/WLayerShell
    protocols/wxwayland.h
    protocols/WXWayland
    protocols/wxwaylandsurface.h
    protocols/WXWaylandSurface
    protocols/wextforeigntoplevellistv1.h
    protocols/ext_foreign_toplevel_image_capture_source_manager_v1.h #TODO: Remove after wlroots 0.20
    protocols/qwextforeigntoplevelimagecapturesourcemanagerv1.h #TODO: Move to qwlroots after wlroots 0.20
)

set(PRIVATE_HEADERS
    platformplugin/qwlrootsintegration.h
    platformplugin/qwlrootscreen.h
    platformplugin/qwlrootswindow.h
    platformplugin/qwlrootscursor.h
    platformplugin/types.h
    kernel/private/wglobal_p.h
    kernel/private/wsurface_p.h
    qtquick/private/woutputviewport_p.h
    qtquick/private/wquickcoordmapper_p.h
    qtquick/private/woutputitem_p.h
    qtquick/private/wquicksocketattached_p.h
    qtquick/private/wqmlcreator_p.h
    qtquick/private/wqmlhelper_p.h
    qtquick/private/wquicktextureproxy_p.h
    qtquick/private/wbufferrenderer_p.h
    qtquick/private/wrenderbuffernode_p.h
    qtquick/private/wsurfaceitem_p.h

    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/text-input-unstable-v1-protocol.h
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/text-input-unstable-v2-protocol.h
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/xdg-output-unstable-v1-protocol.h
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/ext-image-capture-source-v1-protocol.h #TODO: Remove after wlroots 0.20
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/ext-foreign-toplevel-list-v1-protocol.h #TODO: Remove after wlroots 0.20

    protocols/private/winputmethodv2_p.h
    protocols/private/wtextinput_p.h
    protocols/private/wtextinputv1_p.h
    protocols/private/wtextinputv2_p.h
    protocols/private/wtextinputv3_p.h
    protocols/private/wvirtualkeyboardv1_p.h
)

if(NOT DISABLE_XWAYLAND)
    list(APPEND SOURCES
        protocols/wxwayland.cpp
        protocols/wxwaylandsurface.cpp
    )

    list(APPEND HEADERS
        protocols/wxwayland.h
        protocols/wxwaylandsurface.h
    )
endif()

add_library(${TARGET} SHARED ${SOURCES})
add_library(Waylib::WaylibServer ALIAS ${TARGET})

qt_add_qml_module(${TARGET}
    URI Waylib.Server
    VERSION "1.0" # Qt bug: The major version must be larger than 0
    OUTPUT_DIRECTORY "Waylib/Server"
    SOURCES
        ${HEADERS}
        ${PRIVATE_HEADERS}
)

target_compile_definitions(${TARGET}
    PRIVATE
    WLR_USE_UNSTABLE
    LIBWAYLIB_SERVER_LIBRARY
    QT_NO_SIGNALS_SLOTS_KEYWORDS
)

set_target_properties(${TARGET}
    PROPERTIES
        VERSION ${CMAKE_PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        EXPORT_NAME WaylibServer
        PUBLIC_HEADER "${HEADERS}"
        # PRIVATE_HEADER "${PRIVATE_HEADERS}"
        # CMake Warning has PRIVATE_HEADER files but no PRIVATE_HEADER DESTINATION
        POSITION_INDEPENDENT_CODE ON
)

set(QT_LIBRARIES "")
foreach(temp ${QT_COMPONENTS})
    list(APPEND QT_LIBRARIES "Qt6::${temp}")
endforeach()

target_link_libraries(${TARGET}
    PRIVATE
    ${QT_LIBRARIES}
    PkgConfig::WLROOTS
    PkgConfig::LIBDRM
    PkgConfig::WAYLAND
    PkgConfig::WAYLAND_PROTOCOLS
    PkgConfig::PIXMAN
    PkgConfig::XKBCOMMON
    PkgConfig::XCB
    PkgConfig::EGL
)

target_link_libraries(${TARGET}
    PUBLIC
        QWlroots::QWlroots
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

target_include_directories(${TARGET}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/kernel>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qtquick>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qtquick/private>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/protocols>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/protocols/private>
        $<INSTALL_INTERFACE:${WAYLIB_INCLUDE_INSTALL_DIR}>
    PRIVATE
        ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
        ${Qt6Quick_PRIVATE_INCLUDE_DIRS}
        ${Qt6EglSupport_PRIVATE_INCLUDE_DIRS}
        ${Qt6InputSupport_PRIVATE_INCLUDE_DIRS}
)

target_link_directories(${TARGET}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_LIBDIR}>
)

if (WAYLIB_USE_PERCOMPILE_HEADERS)
    target_precompile_headers(${TARGET}
        PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:pch/pch.hxx>"
    )
endif()

install(TARGETS ${TARGET}
    EXPORT
        WaylibServerTargets
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER
        COMPONENT Development
        DESTINATION ${WAYLIB_INCLUDE_INSTALL_DIR}
)

# Install QML module generated targets separately to avoid dependency issues
# This works around the Qt CMake QML module export issue
get_directory_property(all_targets BUILDSYSTEM_TARGETS)
foreach(target_name ${all_targets})
    if(target_name MATCHES "^${TARGET}_resources_")
        install(TARGETS ${target_name}
            EXPORT WaylibServerTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endforeach()

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/WaylibServerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/WaylibServerConfig.cmake"
    INSTALL_DESTINATION ${WAYLIB_CMAKE_INSTALL_DIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/WaylibServerConfigVersion.cmake"
    VERSION ${DTK_VERSION}
    COMPATIBILITY SameMinorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/WaylibServerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/WaylibServerConfigVersion.cmake
    DESTINATION ${WAYLIB_CMAKE_INSTALL_DIR}
)

install(
    EXPORT WaylibServerTargets
    FILE WaylibServerTargets.cmake
    DESTINATION ${WAYLIB_CMAKE_INSTALL_DIR}
    NAMESPACE Waylib::
)

include(${PROJECT_SOURCE_DIR}/cmake/Helpers.cmake)
add_pkgconfig_module(${TARGET} ${TARGET} ${WAYLIB_INCLUDE_INSTALL_DIR}
    "qwlroots, Qt6Gui, Qt6Quick"
)

set(TARGET waylibserver)

set(WAYLIB_CMAKE_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/WaylibServer"
    CACHE STRING "Install directory for waylib cmake files"
)
set(WAYLIB_INCLUDE_INSTALL_DIR
    "${CMAKE_INSTALL_INCLUDEDIR}/${TARGET}"
    CACHE STRING "Install directory for waylib headers"
)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate QuickPrivate)
endif()
find_package(Qt6 COMPONENTS Core Gui Quick REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

if(QT_KNOWN_POLICY_QTP0001) # this policy was introduced in Qt 6.5
    qt_policy(SET QTP0001 NEW)
    # the RESOURCE_PREFIX argument for qt_add_qml_module() defaults to ":/qt/qml/"
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
set(WAYLIB_WLROOTS_INCLUDE_DIRS "")
set(WAYLIB_WLROOTS_PUBLIC_TARGET "")
if (NOT TARGET Wlroots::wlroots)
    # Standalone Waylib build against an installed waylib-wlroots (built by
    # the wlroots CMake scaffold, which also installs its headers and
    # waylib-wlroots.pc). Resolve the library, its public dependencies and
    # the wconfig.h version/feature metadata from that pkg-config file, so
    # no monorepo layout is assumed.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WLR_INSTALLED REQUIRED IMPORTED_TARGET waylib-wlroots)
    add_library(Wlroots::wlroots ALIAS PkgConfig::WLR_INSTALLED)
    set(WAYLIB_WLROOTS_PUBLIC_TARGET PkgConfig::WLR_INSTALLED)
    foreach(_dir IN LISTS WLR_INSTALLED_INCLUDE_DIRS)
        list(APPEND WAYLIB_WLROOTS_INCLUDE_DIRS "$<BUILD_INTERFACE:${_dir}>")
    endforeach()
    pkg_get_variable(WLROOTS_VERSION waylib-wlroots wlroots_version)
    pkg_get_variable(WLR_VERSION_MAJOR waylib-wlroots wlroots_version_major)
    pkg_get_variable(WLR_VERSION_MINOR waylib-wlroots wlroots_version_minor)
    pkg_get_variable(WLR_VERSION_PATCH waylib-wlroots wlroots_version_patch)
    pkg_get_variable(_waylib_wlroots_features waylib-wlroots wlroots_features)
    foreach(_feature IN LISTS _waylib_wlroots_features)
        set(WLR_HAVE_${_feature} 1)
    endforeach()
else()
    set(WAYLIB_WLROOTS_PUBLIC_TARGET Wlroots::wlroots)
    set(_waylib_wlroots_include "${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/wlroots/include")
    if (NOT EXISTS "${_waylib_wlroots_include}/wlr")
        message(FATAL_ERROR "waylib uses vendored wlroots, but headers were not found at ${_waylib_wlroots_include}")
    endif()
    list(APPEND WAYLIB_WLROOTS_INCLUDE_DIRS "$<BUILD_INTERFACE:${_waylib_wlroots_include}>")
endif()
set(WLROOTS_TARGET Wlroots::wlroots)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(WAYLAND_PROTOCOLS REQUIRED IMPORTED_TARGET wayland-protocols)
pkg_search_module(WLR_PROTOCOLS REQUIRED wlr-protocols)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_search_module(XCB REQUIRED IMPORTED_TARGET xcb)
pkg_search_module(EGL REQUIRED IMPORTED_TARGET egl)

find_package(TreelandProtocols REQUIRED)

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

ws_generate(
    server
    wayland-protocols
    staging/security-context/security-context-v1.xml
    security-context-v1-protocol
)

ws_generate(
    server
    wayland-protocols
    stable/xdg-shell/xdg-shell.xml
    xdg-shell-protocol
)

set(SOURCES
    wayliblogging.cpp

    kernel/wbackend.cpp
    kernel/wcursor.cpp
    kernel/winputdevice.cpp
    kernel/woutput.cpp
    kernel/wseat.cpp
    kernel/wevent.cpp
    kernel/wserver.cpp
    kernel/wsurface.cpp
    kernel/wsubsurface.cpp
    kernel/wtoplevelsurface.cpp
    kernel/wwaylandresource.cpp


    kernel/wtypes.cpp

    kernel/woutputlayout.cpp
    kernel/wxcursorimage.cpp
    kernel/wglobal.cpp
    utils/wscoplistener.cpp
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
    qtquick/wbufferitem.cpp

    qtquick/private/wquickcoordmapper.cpp
    qtquick/private/wquicksocketattached.cpp
    qtquick/private/wqmlhelper.cpp
    qtquick/private/wbufferrenderer.cpp
    qtquick/private/wrenderbuffernode.cpp

    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/text-input-unstable-v1-protocol.c
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/text-input-unstable-v2-protocol.c
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/xdg-output-unstable-v1-protocol.c
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/security-context-v1-protocol.c
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/xdg-shell-protocol.c

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
    protocols/wxdgtopleveltagmanager.cpp
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
    protocols/wcursorshapemanagerv1.cpp
    protocols/wpointerconstraintsv1.cpp
    protocols/wrelativepointermanagerv1.cpp
    protocols/woutputmanagerv1.cpp
    protocols/wextforeigntoplevellistv1.cpp
    protocols/wxdgdialogmanagerv1.cpp
    protocols/wsecuritycontextmanager.cpp
    protocols/wsessionlockmanager.cpp
    protocols/wsessionlock.cpp
    protocols/wsessionlocksurface.cpp
    protocols/wremotesubsurfacemanagerv1.cpp
)

set(HEADERS
    wayliblogging.h

    kernel/wglobal.h
    kernel/wwaylandresource.h
    kernel/wlr_all.h
    kernel/wlr_fwd.h
    kernel/wpointer.h
    utils/wscoplistener.h
    utils/wscopedvalue.h
    utils/wlogging.h
    kernel/wbackend.h
    kernel/wcursor.h
    kernel/winputdevice.h
    kernel/woutput.h
    kernel/wseat.h
    kernel/wevent.h
    kernel/wserver.h
    kernel/wsurface.h
    kernel/wsubsurface.h
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
    kernel/WSubsurface

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
    qtquick/wbufferitem.h

    utils/wtools.h
    utils/wthreadutils.h
    utils/WThreadUtils
    utils/wimagebuffer.h
    utils/wcursorimage.h
    utils/WCursorImage
    utils/wextimagecapturesourcev1impl.h
    utils/wbufferdumper.h
    utils/WBufferDumper
    utils/wcontainerof.h
    utils/WContainerOf

    protocols/wxdgshell.h
    protocols/WXdgShell
    protocols/wxdgsurface.h
    protocols/WXdgSurface
    protocols/wxdgtoplevelsurface.h
    protocols/wxdgtopleveltagmanager.h
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
    protocols/wpointerconstraintsv1.h
    protocols/WPointerConstraintsV1
    protocols/wremotesubsurfacemanagerv1.h
    protocols/wrelativepointermanagerv1.h
    protocols/WRelativePointerManagerV1
    protocols/woutputmanagerv1.h
    protocols/WOutputManagerV1
    protocols/wlayershell.h
    protocols/WLayerShell
    protocols/wxwayland.h
    protocols/WXWayland
    protocols/wxwaylandsurface.h
    protocols/WXWaylandSurface
    protocols/wextforeigntoplevellistv1.h
    protocols/wxdgdialogmanagerv1.h
    protocols/wsecuritycontextmanager.h
    protocols/wsessionlockmanager.h
    protocols/WSessionLockManager
    protocols/wsessionlock.h
    protocols/WSessionLock
    protocols/wsessionlocksurface.h
    protocols/WSessionLockSurface
)

set(PRIVATE_HEADERS
    platformplugin/qwlrootsintegration.h
    platformplugin/qwlrootscreen.h
    platformplugin/qwlrootswindow.h
    platformplugin/qwlrootscursor.h
    platformplugin/types.h
    kernel/private/wglobal_p.h
    kernel/private/wwaylandresource_p.h
    kernel/private/wsurface_p.h
    kernel/private/wsubsurface_p.h
    kernel/private/wprivateaccessor_p.h
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
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/xdg-shell-protocol.h

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

waylib_generate_qtwayland_server_protocol(${TARGET}
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-remote-subsurface-unstable-v1.xml
    BASENAME treeland-remote-subsurface-unstable-v1
)

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

# Forward wlroots feature/version vars
# exported by the wlroots CMake build.
if(NOT DEFINED WLROOTS_VERSION)
    message(FATAL_ERROR "WLROOTS_VERSION not set; wlroots CMake export missing")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/wconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/wconfig.h @ONLY)
target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(${TARGET} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

# Public headers (wglobal.h etc.) include Qt types, so the Qt targets must
# propagate to consumers via the exported Waylib::WaylibServer target.
target_link_libraries(${TARGET}
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
    PRIVATE
        Qt6::GuiPrivate
        Qt6::QuickPrivate
        ${WLROOTS_TARGET}
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
        ${WAYLIB_WLROOTS_PUBLIC_TARGET}
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
        ${WAYLIB_WLROOTS_INCLUDE_DIRS}
        $<INSTALL_INTERFACE:${WAYLIB_INCLUDE_INSTALL_DIR}>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
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


# Install the runtime library (versioned .so.*) only; the .so dev
# symlink, exported targets, headers and pkg-config file are installed
# separately when TREELAND_INSTALL_DEV is ON.
install(TARGETS ${TARGET}
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        NAMELINK_SKIP
)

if(TREELAND_INSTALL_DEV)
    install(TARGETS ${TARGET}
        EXPORT
            WaylibServerTargets
        LIBRARY
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            NAMELINK_ONLY
        PUBLIC_HEADER
            COMPONENT Development
            DESTINATION ${WAYLIB_INCLUDE_INSTALL_DIR}
    )

    # Generated config header (wconfig.h) is part of the public API surface
    # (included by wglobal.h) and must ship with the installed headers.
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wconfig.h
            DESTINATION ${WAYLIB_INCLUDE_INSTALL_DIR}
    )

    # Public headers use Q_MOC_INCLUDE/Q_PRIVATE_SLOT referencing kernel
    # private headers (e.g. wserver.h -> private/wserver_p.h), and Treeland
    # consumes kernel private API (e.g. WToplevelSurfacePrivate); install the
    # whole private directory so installed consumers can compile and moc.
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/kernel/private
            DESTINATION ${WAYLIB_INCLUDE_INSTALL_DIR}
            FILES_MATCHING PATTERN "*.h")

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
        VERSION ${PROJECT_VERSION}
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
        "waylib-wlroots, Qt6Gui, Qt6Quick"
    )
endif()

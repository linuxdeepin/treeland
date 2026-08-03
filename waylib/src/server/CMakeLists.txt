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
if (TARGET Wlroots::wlroots)
    # Vendored wlroots from monorepo (wlroots/ subdirectory).
    # WLROOTS_VERSION* and WLR_HAVE_* are exported by wlroots/CMakeLists.txt.
    set(WLROOTS_TARGET Wlroots::wlroots)
    set(WAYLIB_WLROOTS_PUBLIC_TARGET Wlroots::wlroots)
    set(_waylib_wlroots_include "${PROJECT_SOURCE_DIR}/../3rdparty/wlroots/include")
    if (NOT EXISTS "${_waylib_wlroots_include}/wlr")
        message(FATAL_ERROR "Vendored wlroots headers not found at ${_waylib_wlroots_include}")
    endif()
    list(APPEND WAYLIB_WLROOTS_INCLUDE_DIRS "$<BUILD_INTERFACE:${_waylib_wlroots_include}>")
else()
    # System-installed wlroots.
    if (USE_WLROOTS_19)
        set(wlr wlroots-0.19)
        set(WLROOTS_MINIMUM_REQUIRED 0.19.0)
    else()
        set(wlr wlroots-0.18)
        set(WLROOTS_MINIMUM_REQUIRED 0.18.0)
    endif()
    pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET ${wlr}>=${WLROOTS_MINIMUM_REQUIRED})
    pkg_get_variable(WLR_HAVE_DRM_BACKEND ${wlr} have_drm_backend)
    pkg_get_variable(WLR_HAVE_X11_BACKEND ${wlr} have_x11_backend)
    pkg_get_variable(WLR_HAVE_LIBINPUT_BACKEND ${wlr} have_libinput_backend)
    pkg_get_variable(WLR_HAVE_XWAYLAND ${wlr} have_xwayland)
    pkg_get_variable(WLR_HAVE_GLES2_RENDERER ${wlr} have_gles2_renderer)
    pkg_get_variable(WLR_HAVE_VULKAN_RENDERER ${wlr} have_vulkan_renderer)
    pkg_get_variable(WLR_HAVE_GBM_ALLOCATOR ${wlr} have_gbm_allocator)
    pkg_get_variable(WLR_HAVE_SESSION ${wlr} have_session)
    setup_package_version_variables(WLROOTS)
    set(WLROOTS_TARGET PkgConfig::WLROOTS)
    set(WAYLIB_WLROOTS_PUBLIC_TARGET PkgConfig::WLROOTS)
endif()
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(WAYLAND_PROTOCOLS REQUIRED IMPORTED_TARGET wayland-protocols)
if (NOT DISABLE_WLR_PROTOCOLS)
    pkg_search_module(WLR_PROTOCOLS REQUIRED wlr-protocols)
endif()
if ((NOT WLR_HAVE_XWAYLAND) AND (NOT DISABLE_XWAYLAND))
    message(FATAL_ERROR "wlroots lacks support for xwayland, and `DISABLE_XWAYLAND` set to OFF!")
endif()
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_search_module(XCB REQUIRED IMPORTED_TARGET xcb)
pkg_search_module(EGL REQUIRED IMPORTED_TARGET egl)

add_compile_definitions(WLR_PRIVATE=)

# Generate qwconfig.h for qwlroots infrastructure (now built as part of waylib)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/qwlroots/cmake/qwconfig.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generate/qwconfig.h @ONLY)

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

#TODO: Remove after wlroots 0.20
ws_generate(
    server
    wayland-protocols
    staging/xdg-toplevel-tag/xdg-toplevel-tag-v1.xml
    xdg-toplevel-tag-v1-protocol
)

#TODO: Remove after wlroots 0.20
ws_generate(
    server
    wayland-protocols
    stable/xdg-shell/xdg-shell.xml
    xdg-shell-protocol
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

# qwlroots protocol generation (merged from qwlroots/src/CMakeLists.txt)
ws_generate(server wayland-protocols unstable/fullscreen-shell/fullscreen-shell-unstable-v1.xml fullscreen-shell-unstable-v1-protocol)
ws_generate(server wayland-protocols unstable/tablet/tablet-unstable-v2.xml tablet-unstable-v2-protocol)
ws_generate(server wayland-protocols unstable/pointer-constraints/pointer-constraints-unstable-v1.xml pointer-constraints-unstable-v1-protocol)
ws_generate(server wayland-protocols staging/alpha-modifier/alpha-modifier-v1.xml alpha-modifier-v1-protocol)
ws_generate(server wayland-protocols staging/ext-transient-seat/ext-transient-seat-v1.xml ext-transient-seat-v1-protocol)
if (USE_WLROOTS_19)
    ws_generate(server wayland-protocols staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml ext-image-copy-capture-v1-protocol)
endif()
ws_generate(server wayland-protocols staging/content-type/content-type-v1.xml content-type-v1-protocol)
ws_generate(server wayland-protocols staging/cursor-shape/cursor-shape-v1.xml cursor-shape-v1-protocol)
ws_generate(server wayland-protocols staging/tearing-control/tearing-control-v1.xml tearing-control-v1-protocol)
ws_generate(server wayland-protocols stable/tablet/tablet-v2.xml tablet-v2-protocol)

if (NOT DISABLE_WLR_PROTOCOLS)
    ws_generate(server wlr-protocols unstable/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol)
    ws_generate(server wlr-protocols unstable/wlr-output-power-management-unstable-v1.xml wlr-output-power-management-unstable-v1-protocol)
endif()

set(SOURCES
    qwlroots/qwobject.cpp
    qwlroots/qwbackend.cpp
    qwlroots/types/qwinputdevice.cpp

    wayliblogging.cpp

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
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/xdg-toplevel-tag-v1-protocol.c #TODO: Remove after wlroots 0.20
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/xdg-shell-protocol.c #TODO: Remove after wlroots 0.20
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
    protocols/wxdgtopleveltagmanager.cpp #TODO: Remove after wlroots 0.20
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
    protocols/wxdgdialogmanagerv1.cpp
    protocols/wsecuritycontextmanager.cpp
    protocols/wsessionlockmanager.cpp
    protocols/wsessionlock.cpp
    protocols/wsessionlocksurface.cpp
)

set(HEADERS
    wayliblogging.h

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
    qtquick/wbufferitem.h

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
    protocols/ext_foreign_toplevel_image_capture_source_manager_v1.h #TODO: Remove after wlroots 0.20
    protocols/qwextforeigntoplevelimagecapturesourcemanagerv1.h #TODO: Move to qwlroots after wlroots 0.20
    protocols/wsecuritycontextmanager.h
    protocols/wsessionlockmanager.h
    protocols/WSessionLockManager
    protocols/wsessionlock.h
    protocols/WSessionLock
    protocols/wsessionlocksurface.h
    protocols/WSessionLockSurface

    # qwlroots headers (merged from qwlroots/src, now built as part of waylib)
    qwlroots/qwglobal.h
    qwlroots/qwobject.h
    qwlroots/qwbackend.h
    qwlroots/qwdisplay.h
    qwlroots/qwsession.h
    qwlroots/render/qwrenderer.h
    qwlroots/render/qwtexture.h
    qwlroots/render/qwallocator.h
    qwlroots/render/qwegl.h
    qwlroots/render/qwswapchain.h
    qwlroots/render/qwdmabuf.h
    qwlroots/render/qwdrmformatset.h
    qwlroots/render/qwcolor.h
    qwlroots/render/qwdrmsyncobj.h
    qwlroots/types/qwalphamodifierv1.h
    qwlroots/types/qwbuffer.h
    qwlroots/types/qwcompositor.h
    qwlroots/types/qwexportdmabufv1.h
    qwlroots/types/qwsubcompositor.h
    qwlroots/types/qwdatadevice.h
    qwlroots/types/qwdatacontrolv1.h
    qwlroots/types/qwextdatacontrolv1.h
    qwlroots/types/qwdrm.h
    qwlroots/types/qwdrmleasev1.h
    qwlroots/types/qwforeigntoplevelhandlev1.h
    qwlroots/types/qwfractionalscalemanagerv1.h
    qwlroots/types/qwgammacontorlv1.h
    qwlroots/types/qwidleinhibitv1.h
    qwlroots/types/qwidlenotifyv1.h
    qwlroots/types/qwinputdevice.h
    qwlroots/types/qwinputmethodv2.h
    qwlroots/types/qwkeyboard.h
    qwlroots/types/qwkeyboardgroup.h
    qwlroots/types/qwkeyboardshortcutsinhibitv1.h
    qwlroots/types/qwlinuxdmabufv1.h
    qwlroots/types/qwlinuxdrmsyncobjv1.h
    qwlroots/types/qwoutput.h
    qwlroots/types/qwoutputlayer.h
    qwlroots/types/qwoutputlayout.h
    qwlroots/types/qwoutputmanagementv1.h
    qwlroots/types/qwpointer.h
    qwlroots/types/qwpointerconstraintsv1.h
    qwlroots/types/qwpointergesturesv1.h
    qwlroots/types/qwpresentation.h
    qwlroots/types/qwprimaryselection.h
    qwlroots/types/qwprimaryselectionv1.h
    qwlroots/types/qwrelativepointerv1.h
    qwlroots/types/qwscene.h
    qwlroots/types/qwscreencopyv1.h
    qwlroots/types/qwseat.h
    qwlroots/types/qwsecuritycontextmanagerv1.h
    qwlroots/types/qwsessionlockv1.h
    qwlroots/types/qwshm.h
    qwlroots/types/qwsinglepixelbufferv1.h
    qwlroots/types/qwswitch.h
    qwlroots/types/qwtablet.h
    qwlroots/types/qwtabletpad.h
    qwlroots/types/qwtabletv2.h
    qwlroots/types/qwtearingcontrolv1.h
    qwlroots/types/qwtextinputv3.h
    qwlroots/types/qwtouch.h
    qwlroots/types/qwtransientseatv1.h
    qwlroots/types/qwviewporter.h
    qwlroots/types/qwvirtualkeyboardv1.h
    qwlroots/types/qwvirtualpointerv1.h
    qwlroots/types/qwxcursormanager.h
    qwlroots/types/qwxdgactivationv1.h
    qwlroots/types/qwxdgdecorationmanagerv1.h
    qwlroots/types/qwxdgforeignregistry.h
    qwlroots/types/qwxdgforeignv1.h
    qwlroots/types/qwxdgforeignv2.h
    qwlroots/types/qwxdgoutputv1.h
    qwlroots/types/qwxdgshell.h
    qwlroots/types/qwdamagering.h
    qwlroots/types/qwcursor.h
    qwlroots/types/qwcursorshapev1.h
    qwlroots/types/qwcontenttypev1.h
    qwlroots/util/qwsignalconnector.h
    qwlroots/util/qwlogging.h
    qwlroots/util/qwbox.h
    qwlroots/interfaces/qwinterface.h
    qwlroots/interfaces/qwbackendinterface.h
    qwlroots/interfaces/qwbufferinterface.h
    qwlroots/interfaces/qwrendererinterface.h
    qwlroots/interfaces/qwoutputinterface.h
    qwlroots/interfaces/qwkeyboardinterface.h
    qwlroots/interfaces/qwswitchinterface.h
    qwlroots/interfaces/qwpointerinterface.h
    qwlroots/interfaces/qwtabletpadinterface.h
    ${CMAKE_CURRENT_BINARY_DIR}/generate/qwconfig.h
)

if (USE_WLROOTS_19)
    list(APPEND HEADERS
        qwlroots/types/qwcolormanagerv1.h
        qwlroots/types/qwextimagecopycapturev1.h
        qwlroots/types/qwextimagecapturesourcev1.h
        qwlroots/types/qwextforeigntoplevellistv1.h
        qwlroots/types/qwxdgdialogv1.h
        qwlroots/interfaces/qwextimagecapturesourcev1interface.h
    )
else()
    list(APPEND HEADERS
        qwlroots/types/qwfullscreenshellv1.h
    )
endif()

if (NOT DISABLE_XWAYLAND)
    list(APPEND HEADERS
        qwlroots/types/qwxwaylandshellv1.h
        qwlroots/types/qwxwayland.h
        qwlroots/types/qwxwaylandsurface.h
        qwlroots/types/qwxwaylandserver.h
    )
endif()

if (NOT DISABLE_WLR_PROTOCOLS)
    list(APPEND HEADERS
        qwlroots/types/qwlayershellv1.h
        qwlroots/types/qwoutputpowermanagementv1.h
    )
endif()

set(PRIVATE_HEADERS
    platformplugin/qwlrootsintegration.h
    platformplugin/qwlrootscreen.h
    platformplugin/qwlrootswindow.h
    platformplugin/qwlrootscursor.h
    platformplugin/types.h
    kernel/private/wglobal_p.h
    kernel/private/wsurface_p.h
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
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/xdg-toplevel-tag-v1-protocol.h #TODO: Remove after wlroots 0.20
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/xdg-shell-protocol.h #TODO: Remove after wlroots 0.20
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
    QWLROOTS_LIBRARY
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

target_link_libraries(${TARGET}
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qwlroots>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qwlroots/render>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qwlroots/types>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qwlroots/util>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qwlroots/interfaces>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generate>
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/WaylandScannerHelpers.cmake
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
    "Qt6Gui, Qt6Quick"
)

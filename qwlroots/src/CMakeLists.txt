set(TARGET qwlroots)
set(CMAKE_NAME ${CMAKE_PROJECT_NAME})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_compile_options(-Wall -Wextra -Werror=unused-variable -Werror=unused-parameter)

find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
    Core
    REQUIRED
)

find_package(PkgConfig REQUIRED)

if (USE_WLROOTS_19)
    set(wlr wlroots-0.19)
    set(WLROOTS_MINIMUM_REQUIRED 0.19.0)
else ()
    set(wlr wlroots-0.18)
    set(WLROOTS_MINIMUM_REQUIRED 0.18.0)
endif()

pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET ${wlr}>=${WLROOTS_MINIMUM_REQUIRED})
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_check_modules(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)

pkg_get_variable(WLR_HAVE_DRM_BACKEND ${wlr} have_drm_backend)
pkg_get_variable(WLR_HAVE_X11_BACKEND ${wlr} have_x11_backend)
pkg_get_variable(WLR_HAVE_LIBINPUT_BACKEND ${wlr} have_libinput_backend)
pkg_get_variable(WLR_HAVE_XWAYLAND ${wlr} have_xwayland)
pkg_get_variable(WLR_HAVE_GLES2_RENDERER ${wlr} have_gles2_renderer)
pkg_get_variable(WLR_HAVE_VULKAN_RENDERER ${wlr} have_vulkan_renderer)
pkg_get_variable(WLR_HAVE_GBM_ALLOCATOR ${wlr} have_gbm_allocator)
pkg_get_variable(WLR_HAVE_SESSION ${wlr} have_session)
setup_package_version_variables(WLROOTS)
include(CMakePackageConfigHelpers)
configure_file(${PROJECT_SOURCE_DIR}/src/cmake/qwconfig.h.in generate/qwconfig.h @ONLY)

if ((NOT WLR_HAVE_XWAYLAND) AND (NOT DISABLE_XWAYLAND))
    message(FATAL_ERROR "wlroots lacks support for xwayland, and `DISABLE_XWAYLAND` set to OFF!")
endif()

if (DISABLE_WLR_PROTOCOLS)
    set(WLR_PROTOCOLS_CMAKE_CONFIG_INIT "") # for QWlrootsConfig.cmake.in
else()
    pkg_check_modules(WLR_PROTOCOLS wlr-protocols)
    if (NOT WLR_PROTOCOLS_FOUND)
        message(FATAL_ERROR "wlr-protocols not found, and `DISABLE_WLR_PROTOCOLS` set to OFF!")
    endif()
    set(WLR_PROTOCOLS_CMAKE_CONFIG_INIT [[
pkg_check_modules(WLR_PROTOCOLS  wlr-protocols)
ws_generate(server wlr-protocols unstable/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol)
ws_generate(server wlr-protocols unstable/wlr-output-power-management-unstable-v1.xml wlr-output-power-management-unstable-v1-protocol)
    ]])
endif()

message("wlroots version=${WLROOTS_VERSION}, major=${WLROOTS_VERSION_MAJOR}, minor=${WLROOTS_VERSION_MINOR}, patch=${WLROOTS_VERSION_PATCH}")

ws_generate(server wayland-protocols stable/xdg-shell/xdg-shell.xml xdg-shell-protocol)
ws_generate(server wayland-protocols unstable/fullscreen-shell/fullscreen-shell-unstable-v1.xml fullscreen-shell-unstable-v1-protocol)
ws_generate(server wayland-protocols unstable/tablet/tablet-unstable-v2.xml tablet-unstable-v2-protocol)
ws_generate(server wayland-protocols unstable/pointer-constraints/pointer-constraints-unstable-v1.xml pointer-constraints-unstable-v1-protocol)
ws_generate(server wayland-protocols staging/alpha-modifier/alpha-modifier-v1.xml alpha-modifier-v1-protocol)
ws_generate(server wayland-protocols staging/ext-transient-seat/ext-transient-seat-v1.xml ext-transient-seat-v1-protocol)
if (USE_WLROOTS_19)
    ws_generate(server wayland-protocols staging/color-management/color-management-v1.xml color-management-v1-protocol)
    ws_generate(server wayland-protocols staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml ext-image-copy-capture-v1-protocol)
    ws_generate(server wayland-protocols staging/ext-image-capture-source/ext-image-capture-source-v1.xml ext-image-capture-source-v1-protocol)
endif()

set(HEADERS
    qwglobal.h
    qwobject.h
    qwbackend.h
    qwdisplay.h
    render/qwrenderer.h
    render/qwtexture.h
    render/qwallocator.h
    render/qwegl.h
    render/qwswapchain.h
    render/qwdmabuf.h
    render/qwdrmformatset.h
    render/qwcolor.h
    render/qwdrmsyncobj.h
    types/qwalphamodifierv1.h
    types/qwbuffer.h
    types/qwcompositor.h
    types/qwexportdmabufv1.h
    types/qwsubcompositor.h
    types/qwdatadevice.h
    types/qwdatacontrolv1.h
    types/qwextdatacontrolv1.h
    types/qwdamagering.h
    types/qwdrm.h
    types/qwdrmleasev1.h
    types/qwoutputlayout.h
    types/qwscene.h
    types/qwseat.h
    types/qwprimaryselection.h
    types/qwswitch.h
    types/qwxdgshell.h
    types/qwcursor.h
    types/qwxcursormanager.h
    types/qwinputdevice.h
    types/qwtouch.h
    types/qwkeyboard.h
    types/qwpointer.h
    types/qwtablet.h
    types/qwtabletpad.h
    types/qwoutput.h
    types/qwxdgdecorationmanagerv1.h
    types/qwidleinhibitv1.h
    types/qwkeyboardgroup.h
    types/qwpresentation.h
    types/qwxdgforeignregistry.h
    types/qwxdgforeignv1.h
    types/qwxdgforeignv2.h
    types/qwviewporter.h
    types/qwxdgoutputv1.h
    types/qwxdgactivationv1.h
    types/qwpointerconstraintsv1.h
    types/qwtextinputv3.h
    types/qwtabletv2.h
    types/qwsinglepixelbufferv1.h
    types/qwsessionlockv1.h
    types/qwscreencopyv1.h
    types/qwrelativepointerv1.h
    types/qwprimaryselectionv1.h
    types/qwpointergesturesv1.h
    types/qwoutputmanagementv1.h
    types/qwlinuxdmabufv1.h
    types/qwinputmethodv2.h
    types/qwidlenotifyv1.h
    types/qwforeigntoplevelhandlev1.h
    types/qwvirtualkeyboardv1.h
    types/qwvirtualpointerv1.h
    types/qwgammacontorlv1.h
    types/qwkeyboardshortcutsinhibitv1.h
    types/qwfractionalscalemanagerv1.h
    types/qwsecuritycontextmanagerv1.h
    types/qwoutputlayer.h
    types/qwtearingcontrolv1.h
    types/qwshm.h
    types/qwcontenttypev1.h
    types/qwcursorshapev1.h
    types/qwlinuxdrmsyncobjv1.h
    types/qwtransientseatv1.h
    util/qwsignalconnector.h
    util/qwlogging.h
    util/qwbox.h
    interfaces/qwinterface.h
    interfaces/qwbackendinterface.h
    interfaces/qwbufferinterface.h
    interfaces/qwrendererinterface.h
    interfaces/qwoutputinterface.h
    interfaces/qwkeyboardinterface.h
    ${CMAKE_CURRENT_BINARY_DIR}/generate/qwconfig.h
    interfaces/qwswitchinterface.h
    interfaces/qwpointerinterface.h
    interfaces/qwtabletpadinterface.h
)

if (USE_WLROOTS_19)  # 0.19
    list(APPEND HEADERS
        types/qwcolormanagerv1.h
        types/qwextimagecopycapturev1.h
        types/qwextimagecapturesourcev1.h
        types/qwextforeigntoplevellistv1.h
        interfaces/qwextimagecapturesourcev1interface.h
    )
else()
    list(APPEND HEADERS
        types/qwfullscreenshellv1.h
    )
endif()

ws_generate(
    server
    wayland-protocols
    staging/content-type/content-type-v1.xml
    content-type-v1-protocol
)
ws_generate(
    server
    wayland-protocols
    staging/cursor-shape/cursor-shape-v1.xml
    cursor-shape-v1-protocol
)
ws_generate(
    server
    wayland-protocols
    staging/tearing-control/tearing-control-v1.xml
    tearing-control-v1-protocol
)
ws_generate(
    server
    wayland-protocols
    stable/tablet/tablet-v2.xml
    tablet-v2-protocol
)

if (NOT DISABLE_XWAYLAND)
    list(APPEND HEADERS
        types/qwxwaylandshellv1.h
        types/qwxwayland.h
        types/qwxwaylandsurface.h
        types/qwxwaylandserver.h
    )
endif()

if (NOT DISABLE_WLR_PROTOCOLS)
    ws_generate(server wlr-protocols unstable/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol)
    ws_generate(server wlr-protocols unstable/wlr-output-power-management-unstable-v1.xml wlr-output-power-management-unstable-v1-protocol)
    set(PROTOCOL_HEADERS
        types/qwlayershellv1.h
        types/qwoutputpowermanagementv1.h
    )
    list(APPEND HEADERS ${PROTOCOL_HEADERS})
    list(APPEND SOURCES ${PROTOCOL_SOURCES})
endif ()

add_library(${TARGET}
    ${SOURCES}
    ${HEADERS}
    qwobject.cpp
    qwbackend.cpp
    types/qwinputdevice.cpp
)
add_library(QWlroots::QWlroots ALIAS ${TARGET})

set_target_properties(${TARGET}
    PROPERTIES
        VERSION ${CMAKE_PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${HEADERS}"
        EXPORT_NAME ${CMAKE_NAME}
        POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(${TARGET}
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    PkgConfig::WLROOTS
    PkgConfig::WAYLAND_SERVER
    PkgConfig::PIXMAN
    PkgConfig::XKBCOMMON
)

if (QWLROOTS_USE_PERCOMPILE_HEADERS)
    target_precompile_headers(${TARGET}
        PRIVATE
        ${HEADERS}
    )
endif()

target_compile_definitions(${TARGET}
    PRIVATE
    QT_NO_SIGNALS_SLOTS_KEYWORDS
    QWLROOTS_LIBRARY
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

target_include_directories(${TARGET}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generate>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/render>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/types>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/util>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interfaces>
)

target_include_directories(${TARGET}
    INTERFACE
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET}>
)

target_link_directories(${TARGET}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_LIBDIR}>
)

install(TARGETS ${TARGET}
    EXPORT ${CMAKE_NAME}Targets
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER
        COMPONENT Development
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET}
)

include(${PROJECT_SOURCE_DIR}/cmake/Helpers.cmake)
add_cmake_module(${CMAKE_NAME} ${TARGET} ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${TARGET})
add_pkgconfig_module(${TARGET} ${TARGET} ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${TARGET}
    "${wlr} = ${WLROOTS_VERSION}, pixman-1, wayland-protocols, wlr-protocols, xkbcommon, Qt${QT_VERSION_MAJOR}Core"
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_NAME}Config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_NAME}
)

install(FILES
    ${PROJECT_SOURCE_DIR}/cmake/WaylandScannerHelpers.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_NAME}
)

install(EXPORT ${CMAKE_NAME}Targets
    FILE ${CMAKE_NAME}Targets.cmake
    NAMESPACE ${CMAKE_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_NAME}
)

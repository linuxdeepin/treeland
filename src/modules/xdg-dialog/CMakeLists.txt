pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-dialog/xdg-dialog-v1.xml
    BASENAME xdg-dialog-v1
)

impl_treeland(
    NAME
        module_xdg_dialog
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/xdg-dialog/xdgdialogmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/xdg-dialog/xdgdialogmanagerinterfacev1.cpp
        ${CMAKE_BINARY_DIR}/src/modules/xdg-dialog/wayland-xdg-dialog-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

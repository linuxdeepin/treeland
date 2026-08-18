pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml
    BASENAME xdg-activation-v1
)

impl_treeland(
    NAME
        module_activation
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/activation/activationmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/activation/activationmanagerinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

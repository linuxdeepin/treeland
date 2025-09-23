find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-app-id-resolver-v1.xml
    BASENAME treeland-app-id-resolver-v1
)

impl_treeland(
    NAME
        module_app_id_resolver
    SOURCE
        appidresolver.h
        appidresolver.cpp
    ${CMAKE_BINARY_DIR}/src/modules/app-id-resolver/wayland-treeland-app-id-resolver-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    LINK
        Qt6::Core
)

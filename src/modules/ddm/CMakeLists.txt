find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-ddm-v1.xml
    BASENAME treeland-ddm-v1
)

impl_treeland(
    NAME
        module_ddm
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/ddm/ddminterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/ddm/ddminterfacev1.cpp
        ${CMAKE_BINARY_DIR}/src/modules/ddm/wayland-treeland-ddm-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

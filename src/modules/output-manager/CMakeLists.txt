find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-manager-v1.xml
    BASENAME treeland-output-manager-v1
)

impl_treeland(
    NAME
        module_outputmanager
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/output-manager/outputmanagement.h
        ${CMAKE_SOURCE_DIR}/src/modules/output-manager/outputmanagement.cpp
        ${CMAKE_BINARY_DIR}/src/modules/output-manager/wayland-treeland-output-manager-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-input-manager-unstable-v1.xml
    BASENAME treeland-input-manager-unstable-v1
)

impl_treeland(
    NAME
        module_input_manager
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/input-manager/inputmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/input-manager/inputmanagerinterfacev1.cpp
        ${CMAKE_BINARY_DIR}/src/modules/input-manager/wayland-treeland-input-manager-unstable-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

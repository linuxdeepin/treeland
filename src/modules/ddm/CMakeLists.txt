find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-ddm-v2.xml treeland-ddm-v2-protocol)

impl_treeland(
    NAME
        module_ddm
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/ddm/ddminterfacev2.h
        ${CMAKE_SOURCE_DIR}/src/modules/ddm/ddminterfacev2.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-ddm-v2-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

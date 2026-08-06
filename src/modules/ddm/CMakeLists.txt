find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-ddm-v1.xml
    BASENAME treeland-ddm-v1
)

impl_treeland(
    NAME
        module_ddm
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/ddm/ddminterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/ddm/ddminterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

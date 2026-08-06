find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-manager-v1.xml
    BASENAME treeland-output-manager-v1
)

impl_treeland(
    NAME
        module_outputmanager
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/output-manager/outputmanagement.h
        ${CMAKE_SOURCE_DIR}/src/modules/output-manager/outputmanagement.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

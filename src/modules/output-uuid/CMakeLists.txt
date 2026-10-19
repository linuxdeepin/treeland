find_package(TreelandProtocols 0.7.0 REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-uuid-unstable-v1.xml
    BASENAME treeland-output-uuid-unstable-v1
)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-uuid-head-unstable-v1.xml
    BASENAME treeland-output-uuid-head-unstable-v1
)

impl_treeland(
    NAME
        module_output_uuid
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/output-uuid/outputuuidmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/output-uuid/outputuuidmanagerinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

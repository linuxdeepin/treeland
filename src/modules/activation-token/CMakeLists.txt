find_package(TreelandProtocols 0.9.0 REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-activation-token-unstable-v1.xml
    BASENAME treeland-activation-token-unstable-v1
)

impl_treeland(
    NAME
        module_activation_token
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/activation-token/activationtokenmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/activation-token/activationtokenmanagerinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

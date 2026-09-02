find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-app-id-resolver-unstable-v2.xml
    BASENAME treeland-app-id-resolver-unstable-v2
)

impl_treeland(
    NAME
        module_app_id_resolver
    SOURCE
        appidresolver.h
        appidresolver.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    LINK
        Qt6::Core
)

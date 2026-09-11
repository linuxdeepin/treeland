find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-layer-shell-extension-unstable-v1.xml
    BASENAME treeland-layer-shell-extension-unstable-v1
)

impl_treeland(
    NAME
        module_layer_shell_extension
    SOURCE
        layershellextensionmanagerinterfacev1.h
        layershellextensionmanagerinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    LINK
        Qt6::Core
)

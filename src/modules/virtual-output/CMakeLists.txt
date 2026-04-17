find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-virtual-output-manager-v1.xml
    BASENAME treeland-virtual-output-manager-v1
)

impl_treeland(
    NAME
        module_virtualoutput
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/virtual-output/virtualoutputmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/virtual-output/virtualoutputmanagerinterfacev1.cpp
        ${CMAKE_BINARY_DIR}/src/modules/virtual-output/wayland-treeland-virtual-output-manager-v1-server-protocol.c
    LINK
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

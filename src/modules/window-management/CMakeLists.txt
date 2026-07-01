find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-window-management-v1.xml
    BASENAME treeland-window-management-v1
)

impl_treeland(
    NAME
        module_windowmanagement
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/window-management/windowmanagementinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/window-management/windowmanagementinterfacev1.cpp
        ${CMAKE_BINARY_DIR}/src/modules/window-management/wayland-treeland-window-management-v1-server-protocol.c
    LINK
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

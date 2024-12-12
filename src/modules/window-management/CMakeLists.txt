find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-window-management-v1.xml treeland-window-management-protocol)

impl_treeland(
    NAME
        module_windowmanagement
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/window-management/windowmanagement.h
        ${CMAKE_SOURCE_DIR}/src/modules/window-management/impl/window_management_impl.h
        ${CMAKE_SOURCE_DIR}/src/modules/window-management/windowmanagement.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/window-management/impl/window_management_impl.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-window-management-protocol.c
    LINK
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

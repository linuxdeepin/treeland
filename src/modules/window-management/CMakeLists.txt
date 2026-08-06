find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-window-management-v1.xml
    BASENAME treeland-window-management-v1
)

impl_treeland(
    NAME
        module_windowmanagement
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/window-management/windowmanagementinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/window-management/windowmanagementinterfacev1.cpp
    LINK
        Wlroots::wlroots
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

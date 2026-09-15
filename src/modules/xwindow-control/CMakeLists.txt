find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-xwindow-control-unstable-v1.xml
    BASENAME treeland-xwindow-control-unstable-v1
)

impl_treeland(
    NAME
        module_xwindow_control
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/xwindow-control/xwindowcontrolinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/xwindow-control/xwindowcontrolinterfacev1.cpp
    LINK
        Wlroots::wlroots
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

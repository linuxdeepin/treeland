find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-shortcut-manager-v2.xml
    BASENAME treeland-shortcut-manager-v2
)

impl_treeland(
    NAME
        module_shortcut
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutmanager.h
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutmanager.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutrunner.h
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutrunner.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutcontroller.h
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutcontroller.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    LINK
        Wlroots::wlroots
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

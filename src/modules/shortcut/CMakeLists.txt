find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-shortcut-manager-v2.xml treeland-shortcut-manager-protocol)

impl_treeland(
    NAME
        module_shortcut
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/impl/shortcut_manager_impl.h
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/impl/shortcut_manager_impl.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutmanager.h
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutmanager.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutrunner.h
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutrunner.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutcontroller.h
        ${CMAKE_SOURCE_DIR}/src/modules/shortcut/shortcutcontroller.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-shortcut-manager-protocol.c
    LINK
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

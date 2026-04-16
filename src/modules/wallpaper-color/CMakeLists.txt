find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-color-v1.xml
    BASENAME treeland-wallpaper-color-v1
)

impl_treeland(
    NAME
        module_wallpapercolor
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper-color/wallpapercolorinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper-color/wallpapercolorinterfacev1.cpp
        ${CMAKE_BINARY_DIR}/src/modules/wallpaper-color/wayland-treeland-wallpaper-color-v1-server-protocol.c
    LINK
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

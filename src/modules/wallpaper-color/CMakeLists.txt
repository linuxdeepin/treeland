find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-color-v1.xml treeland-wallpaper-color-protocol)

impl_treeland(
    NAME
        module_wallpapercolor
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper-color/wallpapercolor.h
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper-color/impl/wallpaper_color_impl.h
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper-color/wallpapercolor.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper-color/impl/wallpaper_color_impl.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-wallpaper-color-protocol.c
    LINK
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

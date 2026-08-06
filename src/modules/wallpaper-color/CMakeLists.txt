find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-color-v1.xml
    BASENAME treeland-wallpaper-color-v1
)

impl_treeland(
    NAME
        module_wallpapercolor
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper-color/wallpapercolorinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper-color/wallpapercolorinterfacev1.cpp
    LINK
        Wlroots::wlroots
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

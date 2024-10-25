set(MODULE_NAME wallpaper-color)

find_package(TreeLandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-color-v1.xml treeland-wallpaper-color-protocol)

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

qt_add_library(${MODULE_NAME} STATIC)

target_sources(${MODULE_NAME} PUBLIC
FILE_SET HEADERS
FILES
    wallpapercolor.h
    impl/wallpaper_color_impl.h
)

target_sources(${MODULE_NAME} PRIVATE
    wallpapercolor.cpp
    impl/wallpaper_color_impl.cpp
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-wallpaper-color-protocol.c
)

target_include_directories(${MODULE_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_definitions(${MODULE_NAME}
    PRIVATE
    WLR_USE_UNSTABLE
)

target_link_libraries(${MODULE_NAME}
    PUBLIC
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

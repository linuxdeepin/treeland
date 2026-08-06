find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-manager-unstable-v1.xml
    BASENAME treeland-wallpaper-manager-unstable-v1
)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-shell-unstable-v1.xml
    BASENAME treeland-wallpaper-shell-unstable-v1
)

impl_treeland(
    NAME
        module_wallpaper
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper/wallpapermanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper/wallpapermanagerinterfacev1.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper/wallpapershellinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper/wallpapershellinterfacev1.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper/wallpapernotifierinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/wallpaper/wallpapernotifierinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

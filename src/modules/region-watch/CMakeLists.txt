find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-region-watch-unstable-v1.xml
    BASENAME treeland-region-watch-unstable-v1
)

impl_treeland(
    NAME
        module_region_watch
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/region-watch/regionwatchmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/region-watch/regionwatchmanagerinterfacev1.cpp
    LINK
        Wlroots::wlroots
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

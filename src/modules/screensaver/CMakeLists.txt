find_package(TreelandProtocols 0.6.0 REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-screensaver-unstable-v2.xml
    BASENAME treeland-screensaver-unstable-v2
)

impl_treeland(
    NAME
        module_screensaver
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/screensaver/screensaverinterfacev2.h
        ${CMAKE_SOURCE_DIR}/src/modules/screensaver/screensaverinterfacev2.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

find_package(TreelandProtocols 0.5.3 REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-screensaver-v1.xml
    BASENAME treeland-screensaver-v1
)

impl_treeland(
    NAME
        module_screensaver
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/screensaver/screensaverinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/screensaver/screensaverinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

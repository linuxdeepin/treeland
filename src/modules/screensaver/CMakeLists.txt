find_package(TreelandProtocols 0.5.3 REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-screensaver-v1.xml treeland-screensaver-v1-protocol)

impl_treeland(
    NAME
        module_screensaver
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/screensaver/screensaverinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/screensaver/screensaverinterfacev1.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-screensaver-v1-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

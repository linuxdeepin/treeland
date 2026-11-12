
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/protocols/treeland-screensaver-unstable-v2.xml
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

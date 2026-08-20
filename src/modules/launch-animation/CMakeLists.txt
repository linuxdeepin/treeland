find_package(TreelandProtocols 0.5.11 REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-launch-animation-v1.xml
    BASENAME treeland-launch-animation-v1
)

impl_treeland(
    NAME
        module_launch_animation
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/launch-animation/launchanimationmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/launch-animation/launchanimationmanagerinterfacev1.cpp
        ${CMAKE_BINARY_DIR}/src/modules/launch-animation/wayland-treeland-launch-animation-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    LINK
        Qt6::Core
)

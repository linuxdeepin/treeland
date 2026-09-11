find_package(TreelandProtocols 0.5.10 REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-window-transition-unstable-v1.xml
    BASENAME treeland-window-transition-unstable-v1
)

impl_treeland(
    NAME
        module_window_transition
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/window-transition/windowtransitionmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/window-transition/windowtransitionmanagerinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    LINK
        Qt6::Core
)

find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-active-notify-unstable-v1.xml
    BASENAME treeland-active-notify-unstable-v1
)

impl_treeland(
    NAME
        module_active_notify
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/active-notify/activenotifymanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/active-notify/activenotifymanagerinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-keyboard-state-notify-unstable-v1.xml
    BASENAME treeland-keyboard-state-notify-unstable-v1
)

impl_treeland(
    NAME
        module_keyboard_state_notify
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/keyboard-state-notify/keyboardstatenotifymanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/keyboard-state-notify/keyboardstatenotifymanagerinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

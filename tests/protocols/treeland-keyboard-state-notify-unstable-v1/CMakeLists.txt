
treeland_add_protocol_test(
    NAME treeland_keyboard_state_notify_unstable_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-keyboard-state-notify-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-keyboard-state-notify-unstable-v1.c"
)

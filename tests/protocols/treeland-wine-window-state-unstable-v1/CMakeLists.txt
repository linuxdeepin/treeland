treeland_add_protocol_test(
    NAME treeland_wine_window_state_unstable_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wine-window-state-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-wine-window-state-unstable-v1.c"
)

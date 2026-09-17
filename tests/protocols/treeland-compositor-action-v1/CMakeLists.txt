treeland_add_protocol_test(
    NAME treeland_compositor_action_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-compositor-action-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-compositor-action-v1.c"
)

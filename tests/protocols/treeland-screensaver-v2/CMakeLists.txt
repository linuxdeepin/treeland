treeland_add_protocol_test(
    NAME treeland_screensaver_v2
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-screensaver-unstable-v2.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-screensaver-v2.c"
)

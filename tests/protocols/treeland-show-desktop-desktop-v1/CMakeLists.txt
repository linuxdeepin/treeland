
treeland_add_protocol_test(
    NAME treeland_show_desktop_desktop_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-show-desktop-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-show-desktop-desktop-v1.c"
)

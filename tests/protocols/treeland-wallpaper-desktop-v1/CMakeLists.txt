
treeland_add_protocol_test(
    NAME treeland_wallpaper_desktop_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-manager-unstable-v1.xml"
    EXTRA_XMLS "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-shell-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-wallpaper-desktop-v1.c"
)

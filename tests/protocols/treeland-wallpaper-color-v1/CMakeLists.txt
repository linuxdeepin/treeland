treeland_add_protocol_test(
    NAME treeland_wallpaper_color_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-color-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-wallpaper-color-v1.c"
)

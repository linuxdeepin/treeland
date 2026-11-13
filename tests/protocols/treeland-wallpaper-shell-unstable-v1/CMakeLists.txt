treeland_add_protocol_test(
    NAME treeland_wallpaper_shell_unstable_v1
    XML "${PROJECT_SOURCE_DIR}/src/protocols/treeland-wallpaper-shell-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-wallpaper-shell-unstable-v1.c"
)

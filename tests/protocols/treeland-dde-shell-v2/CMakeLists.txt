treeland_add_protocol_test(
    NAME treeland_dde_shell_v2
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-unstable-v2.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-dde-shell-v2.c"
)

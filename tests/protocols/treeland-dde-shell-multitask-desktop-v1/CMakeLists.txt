
treeland_add_protocol_test(
    NAME treeland_dde_shell_multitask_desktop_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-dde-shell-multitask-desktop-v1.c"
)

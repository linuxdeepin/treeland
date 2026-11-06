treeland_add_protocol_test(
    NAME treeland_activation_token_unstable_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-activation-token-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-activation-token-unstable-v1.c"
    EXTRA_XMLS
        "${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml"
        "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-prelaunch-splash-unstable-v2.xml"
)

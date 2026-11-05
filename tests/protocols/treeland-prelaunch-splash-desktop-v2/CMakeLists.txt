
treeland_add_protocol_test(
    NAME treeland_prelaunch_splash_desktop_v2
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-prelaunch-splash-unstable-v2.xml"
    EXTRA_XMLS
        "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-app-id-resolver-unstable-v2.xml"
        "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-unstable-v3.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-prelaunch-splash-desktop-v2.c"
)

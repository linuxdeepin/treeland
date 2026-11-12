
treeland_add_protocol_test(
    NAME treeland_app_id_resolver_desktop_v2
    XML "${PROJECT_SOURCE_DIR}/src/protocols/treeland-app-id-resolver-unstable-v2.xml"
    EXTRA_XMLS "${PROJECT_SOURCE_DIR}/src/protocols/treeland-prelaunch-splash-unstable-v2.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-app-id-resolver-desktop-v2.c"
)

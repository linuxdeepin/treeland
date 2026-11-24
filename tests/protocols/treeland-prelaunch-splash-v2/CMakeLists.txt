treeland_add_protocol_test(
    NAME treeland_prelaunch_splash_v2
    XML "${PROJECT_SOURCE_DIR}/src/protocols/treeland-prelaunch-splash-unstable-v2.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-prelaunch-splash-v2.c"
)

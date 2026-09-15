
treeland_add_protocol_test(
    NAME treeland_xwindow_control_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-xwindow-control-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-xwindow-control-unstable-v1.c"
)

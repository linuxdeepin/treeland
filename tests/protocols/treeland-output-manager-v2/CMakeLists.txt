treeland_add_protocol_test(
    NAME treeland_output_manager_v2
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-manager-unstable-v2.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-output-manager-v2.c"
)

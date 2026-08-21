treeland_add_protocol_test(
    NAME treeland_output_manager_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-manager-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-output-manager-v1.c"
)

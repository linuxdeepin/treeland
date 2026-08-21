
treeland_add_protocol_test(
    NAME treeland_input_manager_uinput_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-input-manager-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-input-manager-uinput-v1.c"
)

set_tests_properties(test_treeland_input_manager_uinput_v1 PROPERTIES
    ENVIRONMENT "TREELAND_TEST_WLR_BACKENDS=headless,libinput;WLR_RENDERER=pixman;DSG_DATA_DIRS=${TREELAND_PROTOCOL_TEST_DSG_DATA_DIRS}"
    SKIP_RETURN_CODE 77
)

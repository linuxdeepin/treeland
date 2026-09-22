
treeland_add_protocol_test(
    NAME treeland_input_manager_uinput_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-input-manager-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-input-manager-uinput-v1.c"
)

set_tests_properties(test_treeland_input_manager_uinput_v1 PROPERTIES
    ENVIRONMENT "WLR_BACKENDS=headless,libinput;WLR_RENDERER=pixman;LIBSEAT_BACKEND=seatd;SEATD_SOCK=/run/dde-seatd.sock;WLR_XWAYLAND=$<TARGET_FILE:treeland-xwayland>;DSG_DATA_DIRS=${TREELAND_PROTOCOL_TEST_DSG_DATA_DIRS};TREELAND_PROTOCOL_TEST_DSG_DIR=${TREELAND_PROTOCOL_TEST_DSG_DATA_DIR}"
    SKIP_RETURN_CODE 77
)

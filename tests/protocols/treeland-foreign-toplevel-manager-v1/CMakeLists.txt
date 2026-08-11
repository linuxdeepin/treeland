treeland_add_desktop_integration_test(
    NAME treeland_foreign_toplevel_manager_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-foreign-toplevel-manager-v1.c"
)

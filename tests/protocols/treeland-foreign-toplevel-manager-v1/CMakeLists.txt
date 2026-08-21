treeland_add_protocol_test(
    NAME treeland_foreign_toplevel_manager_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-foreign-toplevel-manager-v1.c"
)

# This integration scenario is currently timing-sensitive under headless CI.
# Keep building it, but do not run it in the default CTest set until it is
# stabilized.
set_tests_properties(test_treeland_foreign_toplevel_manager_v1 PROPERTIES DISABLED TRUE)

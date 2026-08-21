
treeland_add_protocol_test(
    NAME treeland_app_id_resolver_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-app-id-resolver-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-app-id-resolver-v1.c"
)

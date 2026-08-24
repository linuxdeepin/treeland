treeland_add_protocol_test(
    NAME ext-foreign-toplevel-list-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/ext-foreign-toplevel-list-v1.c"
)

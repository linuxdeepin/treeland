treeland_add_protocol_test(
    NAME ext-data-control-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-data-control/ext-data-control-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/ext-data-control-v1.c"
)

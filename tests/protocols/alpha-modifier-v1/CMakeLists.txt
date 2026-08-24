treeland_add_protocol_test(
    NAME alpha-modifier-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/alpha-modifier/alpha-modifier-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/alpha-modifier-v1.c"
)

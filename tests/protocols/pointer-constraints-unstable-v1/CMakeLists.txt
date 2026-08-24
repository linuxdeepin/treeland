treeland_add_protocol_test(
    NAME pointer-constraints-unstable-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/pointer-constraints-unstable-v1.c"
)

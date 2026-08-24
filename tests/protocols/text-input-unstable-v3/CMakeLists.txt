treeland_add_protocol_test(
    NAME text-input-unstable-v3
    XML "${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v3.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/text-input-unstable-v3.c"
)

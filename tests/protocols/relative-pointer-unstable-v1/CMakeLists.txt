treeland_add_protocol_test(
    NAME relative-pointer-unstable-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/relative-pointer-unstable-v1.c"
)

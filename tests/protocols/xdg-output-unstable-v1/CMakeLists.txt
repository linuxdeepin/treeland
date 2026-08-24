treeland_add_protocol_test(
    NAME xdg-output-unstable-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/xdg-output-unstable-v1.c"
)

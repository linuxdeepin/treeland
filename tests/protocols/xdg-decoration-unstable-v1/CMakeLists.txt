treeland_add_protocol_test(
    NAME xdg-decoration-unstable-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/xdg-decoration-unstable-v1.c"
)

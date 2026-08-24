treeland_add_protocol_test(
    NAME xdg-foreign-unstable-v2
    XML "${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/xdg-foreign-unstable-v2.c"
)

treeland_add_protocol_test(
    NAME xdg-toplevel-tag-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-toplevel-tag/xdg-toplevel-tag-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/xdg-toplevel-tag-v1.c"
)

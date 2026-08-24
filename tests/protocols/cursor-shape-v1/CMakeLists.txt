treeland_add_protocol_test(
    NAME cursor-shape-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml"
    EXTRA_XMLS "${WAYLAND_PROTOCOLS_DATADIR}/unstable/tablet/tablet-unstable-v2.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/cursor-shape-v1.c"
)

treeland_add_protocol_test(
    NAME viewporter
    XML "${WAYLAND_PROTOCOLS_DATADIR}/stable/viewporter/viewporter.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/viewporter.c"
)

treeland_add_protocol_test(
    NAME idle-inhibit-unstable-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/idle-inhibit-unstable-v1.c"
)

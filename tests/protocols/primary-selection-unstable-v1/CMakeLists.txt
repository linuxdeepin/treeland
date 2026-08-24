treeland_add_protocol_test(
    NAME primary-selection-unstable-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/unstable/primary-selection/primary-selection-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/primary-selection-unstable-v1.c"
)

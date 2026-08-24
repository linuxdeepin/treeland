treeland_add_protocol_test(
    NAME pointer-gestures-unstable-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/pointer-gestures-unstable-v1.c"
)

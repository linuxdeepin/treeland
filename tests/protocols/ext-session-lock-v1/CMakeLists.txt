treeland_add_protocol_test(
    NAME ext-session-lock-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-session-lock/ext-session-lock-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/ext-session-lock-v1.c"
)

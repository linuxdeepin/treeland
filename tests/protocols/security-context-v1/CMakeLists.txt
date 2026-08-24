treeland_add_protocol_test(
    NAME security-context-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/security-context/security-context-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/security-context-v1.c"
)

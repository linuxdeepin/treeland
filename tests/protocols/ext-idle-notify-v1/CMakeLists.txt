treeland_add_protocol_test(
    NAME ext-idle-notify-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-idle-notify/ext-idle-notify-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/ext-idle-notify-v1.c"
)

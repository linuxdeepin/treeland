treeland_add_protocol_test(
    NAME fractional-scale-v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/fractional-scale-v1.c"
)

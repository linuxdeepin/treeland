
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

treeland_add_protocol_test(
    NAME treeland_screensaver_desktop_v2
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-screensaver-unstable-v2.xml"
    EXTRA_XMLS "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-idle-notify/ext-idle-notify-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-screensaver-desktop-v2.c"
)

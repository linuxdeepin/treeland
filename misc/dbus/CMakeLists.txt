configure_file(
    "org.deepin.compositor1.conf.in"
    "org.deepin.compositor1.conf"
    IMMEDIATE @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/org.deepin.compositor1.conf"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/system.d"
)

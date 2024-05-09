install(FILES
    "dbus/org.deepin.compositor1.conf"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/system.d"
)

add_subdirectory(systemd)
add_subdirectory(wayland-sessions)

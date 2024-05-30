install(FILES
    "dbus/org.deepin.compositor1.conf"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/system.d"
)

install(FILES
    "dconfig/org.deepin.Treeland.json"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dsg/configs/org.deepin.Treeland"
)

add_subdirectory(systemd)
add_subdirectory(wayland-sessions)

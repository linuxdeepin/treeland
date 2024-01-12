configure_file(treeland.desktop.in treeland.desktop)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/treeland.desktop
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions"
)

configure_file(treeland-user.desktop.in treeland-user.desktop)
configure_file(treeland.desktop.in treeland.desktop)
configure_file(treeland-user-wrapper.in treeland-user-wrapper)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/treeland.desktop
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions"
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/treeland-user.desktop
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions"
)

install(PROGRAMS
    ${CMAKE_CURRENT_BINARY_DIR}/treeland-user-wrapper
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

macro(install_symlink filepath wantsdir)
    file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/link/${wantsdir}/)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_INSTALL_PREFIX}/lib/systemd/user/${filepath} ${PROJECT_BINARY_DIR}/link/${wantsdir}/${filepath})
    install(FILES ${PROJECT_BINARY_DIR}/link/${wantsdir}/${filepath} DESTINATION lib/systemd/user/${wantsdir}/)
endmacro(install_symlink)

configure_file("treeland-sd.service.in" "treeland-sd.service" IMMEDIATE @ONLY)
configure_file("dde-session-shortcut.service.in" "dde-session-shortcut.service" IMMEDIATE @ONLY)

set(SERVICES
    treeland.service
    treeland-sd.socket
    ${CMAKE_CURRENT_BINARY_DIR}/treeland-sd.service
    ${CMAKE_CURRENT_BINARY_DIR}/dde-session-shortcut.service
)
install(FILES ${SERVICES} DESTINATION lib/systemd/user/)

install_symlink(treeland.service dde-session.target.wants)
install_symlink(dde-session-shortcut.service dde-session.target.wants)

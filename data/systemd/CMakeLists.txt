macro(install_symlink filepath wantsdir)
    file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/link/${wantsdir}/)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_INSTALL_PREFIX}/lib/systemd/user/${filepath} ${PROJECT_BINARY_DIR}/link/${wantsdir}/${filepath})
    install(FILES ${PROJECT_BINARY_DIR}/link/${wantsdir}/${filepath} DESTINATION lib/systemd/user/${wantsdir}/)
endmacro(install_symlink)

set(SERVICES
    dde-fake-session-initialized.target
    dde-fake-session.target
)
install(FILES ${SERVICES} DESTINATION lib/systemd/user/)

install(DIRECTORY DESTINATION lib/systemd/user/dde-fake-session-initialized.target.wants/)
set(DDE_SESSION_INITIALIZED_WANTS
    dde-fake-session-initialized.target.wants/fake-dde-desktop.service
    dde-fake-session-initialized.target.wants/fake-dde.service
    dde-fake-session-initialized.target.wants/fake-dde-polkit-agent.service
    dde-fake-session-initialized.target.wants/fake-dde-am.service
    dde-fake-session-initialized.target.wants/fake-dde-im.service
    dde-fake-session-initialized.target.wants/fake-dde-osd.service
)

install(FILES ${DDE_SESSION_PRE_WANTS} DESTINATION lib/systemd/user/)
install(FILES ${DDE_SESSION_INITIALIZED_WANTS} DESTINATION lib/systemd/user/)
install_symlink(fake-dde-desktop.service dde-fake-session-initialized.target.wants)
install_symlink(fake-dde.service dde-fake-session-initialized.target.wants)
install_symlink(fake-dde-polkit-agent.service dde-fake-session-initialized.target.wants)
install_symlink(fake-dde-am.service dde-fake-session-initialized.target.wants)
install_symlink(fake-dde-im.service dde-fake-session-initialized.target.wants)
install_symlink(fake-dde-osd.service dde-fake-session-initialized.target.wants)

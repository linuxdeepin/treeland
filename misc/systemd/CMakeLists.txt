if (NOT DEFINED SYSTEMD_SYSTEM_UNIT_DIR)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Systemd REQUIRED systemd)
    pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemdsystemunitdir)
endif()

if (NOT DEFINED SYSTEMD_USER_UNIT_DIR)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Systemd REQUIRED systemd)
    pkg_get_variable(SYSTEMD_USER_UNIT_DIR systemd systemduserunitdir)
endif()

macro(install_symlink filepath wantsdir)
    file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/link/${wantsdir}/)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_INSTALL_PREFIX}/lib/systemd/user/${filepath} ${PROJECT_BINARY_DIR}/link/${wantsdir}/${filepath})
    install(FILES ${PROJECT_BINARY_DIR}/link/${wantsdir}/${filepath} DESTINATION ${SYSTEMD_USER_UNIT_DIR}/${wantsdir}/)
endmacro(install_symlink)

configure_file("dde-session-pre.target.wants/treeland-sd.service.in" "dde-session-pre.target.wants/treeland-sd.service" IMMEDIATE @ONLY)
if (NOT DISABLE_XWAYLAND)
    configure_file("dde-session-pre.target.wants/treeland-xwayland.service.in" "dde-session-pre.target.wants/treeland-xwayland.service" IMMEDIATE @ONLY)
    set(TREELAND_XWAYLAND_SERVICE_DEPS "Wants=treeland-xwayland.service\nBefore=treeland-xwayland.service")
else()
    set(TREELAND_XWAYLAND_SERVICE_DEPS "")
endif()
configure_file("dde-session-pre.target.wants/treeland.service.in" "dde-session-pre.target.wants/treeland.service" IMMEDIATE @ONLY)
configure_file("treeland-session-helper.service.in" "treeland-session-helper.service" IMMEDIATE @ONLY)
configure_file("treeland-screensaver.service.in" "treeland-screensaver.service" IMMEDIATE @ONLY)
configure_file("treeland.service.in" "treeland.service" IMMEDIATE @ONLY)

install(FILES ${SERVICES} DESTINATION ${SYSTEMD_USER_UNIT_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/treeland.service DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/treeland-session-helper.service DESTINATION ${SYSTEMD_USER_UNIT_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/treeland-screensaver.service DESTINATION ${SYSTEMD_USER_UNIT_DIR})

install(DIRECTORY DESTINATION ${SYSTEMD_USER_UNIT_DIR}/dde-session-pre.target.wants/)
install(DIRECTORY DESTINATION ${SYSTEMD_USER_UNIT_DIR}/dde-session-shutdown.target.wants/)
set(DDE_SESSION_PRE_WANTS
    dde-session-pre.target.wants/treeland-sd.socket
    dde-session-shutdown.target.wants/treeland-session-shutdown.service
    ${CMAKE_CURRENT_BINARY_DIR}/dde-session-pre.target.wants/treeland-sd.service
    ${CMAKE_CURRENT_BINARY_DIR}/dde-session-pre.target.wants/treeland.service
)
if (NOT DISABLE_XWAYLAND)
    list(APPEND DDE_SESSION_PRE_WANTS
        dde-session-pre.target.wants/treeland-xwayland.socket
        ${CMAKE_CURRENT_BINARY_DIR}/dde-session-pre.target.wants/treeland-xwayland.service
    )
endif()

install(FILES ${DDE_SESSION_PRE_WANTS} DESTINATION ${SYSTEMD_USER_UNIT_DIR})
install_symlink(treeland-sd.socket dde-session-pre.target.wants)
install_symlink(treeland-sd.service dde-session-pre.target.wants)
if (NOT DISABLE_XWAYLAND)
    install_symlink(treeland-xwayland.socket dde-session-pre.target.wants)
    install_symlink(treeland-xwayland.service dde-session-pre.target.wants)
endif()
install_symlink(treeland.service dde-session-pre.target.wants)
install_symlink(treeland-session-shutdown.service dde-session-shutdown.target.wants)
install_symlink(treeland-session-helper.service treeland.service.wants)
install_symlink(treeland-screensaver.service treeland.service.wants)

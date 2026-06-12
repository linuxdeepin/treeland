find_package(PkgConfig REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

set(PROTOCOL_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Generate xdg-toplevel-tag-v1 client code
set(TAG_PROTOCOL ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-toplevel-tag/xdg-toplevel-tag-v1.xml)
set(TAG_CLIENT_H ${PROTOCOL_DIR}/xdg-toplevel-tag-v1-client-protocol.h)
set(TAG_CLIENT_CODE ${PROTOCOL_DIR}/xdg-toplevel-tag-v1-client-protocol.c)

add_custom_command(
    OUTPUT ${TAG_CLIENT_H} ${TAG_CLIENT_CODE}
    COMMAND ${WAYLAND_SCANNER} private-code < ${TAG_PROTOCOL} > ${TAG_CLIENT_CODE}
    COMMAND ${WAYLAND_SCANNER} client-header < ${TAG_PROTOCOL} > ${TAG_CLIENT_H}
    DEPENDS ${TAG_PROTOCOL}
    COMMENT "Generating xdg-toplevel-tag-v1 protocol"
    VERBATIM
)

# Generate xdg-shell client code
set(XDG_SHELL_PROTOCOL ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml)
set(XDG_SHELL_CLIENT_H ${PROTOCOL_DIR}/xdg-shell-client-protocol.h)
set(XDG_SHELL_CLIENT_CODE ${PROTOCOL_DIR}/xdg-shell-client-protocol.c)

add_custom_command(
    OUTPUT ${XDG_SHELL_CLIENT_H} ${XDG_SHELL_CLIENT_CODE}
    COMMAND ${WAYLAND_SCANNER} private-code < ${XDG_SHELL_PROTOCOL} > ${XDG_SHELL_CLIENT_CODE}
    COMMAND ${WAYLAND_SCANNER} client-header < ${XDG_SHELL_PROTOCOL} > ${XDG_SHELL_CLIENT_H}
    DEPENDS ${XDG_SHELL_PROTOCOL}
    COMMENT "Generating xdg-shell protocol"
    VERBATIM
)

set(BIN_NAME test-im-candidate-panel-tag)

add_executable(${BIN_NAME} main.c ${TAG_CLIENT_CODE} ${XDG_SHELL_CLIENT_CODE})

target_include_directories(${BIN_NAME} PRIVATE ${PROTOCOL_DIR})
target_link_libraries(${BIN_NAME} PRIVATE ${WAYLAND_CLIENT_LIBRARIES})

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

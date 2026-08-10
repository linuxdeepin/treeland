treeland_add_protocol_test(
    NAME treeland_foreign_toplevel_manager_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-foreign-toplevel-manager-v1.c"
)

# The dock preview context only resolves surfaces that carry a waylib WSurface
# wrapper, which exists only for shell surfaces; the client therefore creates
# an xdg_toplevel, so generate the xdg-shell client bindings here.
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
set(XDG_SHELL_XML "${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml")
set(XDG_SHELL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
set(XDG_SHELL_CODE "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.c")
add_custom_command(
    OUTPUT "${XDG_SHELL_HEADER}"
    COMMAND wayland-scanner client-header "${XDG_SHELL_XML}" "${XDG_SHELL_HEADER}"
    DEPENDS "${XDG_SHELL_XML}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${XDG_SHELL_CODE}"
    COMMAND wayland-scanner private-code "${XDG_SHELL_XML}" "${XDG_SHELL_CODE}"
    DEPENDS "${XDG_SHELL_XML}" "${XDG_SHELL_HEADER}"
    VERBATIM
)
target_sources(test_treeland_foreign_toplevel_manager_v1 PRIVATE
    "${XDG_SHELL_HEADER}"
    "${XDG_SHELL_CODE}"
)

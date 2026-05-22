find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate WaylandClientPrivate)
endif()
find_package(PkgConfig REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)


set(BIN_NAME test-toplevel-tag)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-toplevel-tag/xdg-toplevel-tag-v1.xml
)

# xdg-toplevel-tag references xdg_toplevel_interface from xdg-shell
qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::Widgets
        Qt6::WaylandClient
        Qt6::GuiPrivate
        Qt6::WaylandClientPrivate
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

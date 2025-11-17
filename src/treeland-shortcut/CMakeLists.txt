find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets DBus)
find_package(Qt6 COMPONENTS WaylandClientPrivate QUIET)
find_package(TreelandProtocols REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)
pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)

qt_add_executable(treeland-shortcut
    shortcut.h
    shortcut.cpp
)

qt_generate_wayland_protocol_client_sources(treeland-shortcut
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-shortcut-manager-v2.xml
)

target_link_libraries(treeland-shortcut
    PRIVATE
        Waylib::WaylibServer
        Qt6::Widgets
        Qt6::DBus
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
        PkgConfig::WAYLAND_CLIENT
        PkgConfig::Systemd
)

install(TARGETS treeland-shortcut RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
install(DIRECTORY "shortcuts" DESTINATION "${TREELAND_DATA_DIR}")

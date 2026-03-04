set(BIN_NAME test_idle_inhibit_v1)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WaylandClient)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate WaylandClientPrivate)
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED IMPORTED_TARGET wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

qt_add_executable(${BIN_NAME}
    main.cpp
    idleclient.h
    idleclient.cpp
    idleinhibitv1.h
    idleinhibitv1.cpp
    idlenotifyv1.h
    idlenotifyv1.cpp
)

qt6_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-idle-notify/ext-idle-notify-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Widgets
    Qt6::WaylandClient
    Qt6::GuiPrivate
    Qt6::WaylandClientPrivate
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

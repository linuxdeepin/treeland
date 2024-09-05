set(BIN_NAME test_data_control_manager)

find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WLR_PROTOCOLS REQUIRED IMPORTED_TARGET wlr-protocols)
pkg_get_variable(WLR_PROTOCOLS_DATADIR wlr-protocols pkgdatadir)

qt_add_executable(${BIN_NAME}
    main.cpp
    clipboard.h
    clipboard.cpp
    datacontrolmanagerv1.h
    datacontrolmanagerv1.cpp
)

qt6_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${WLR_PROTOCOLS_DATADIR}/unstable/wlr-data-control-unstable-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::WaylandClient
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

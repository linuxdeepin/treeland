find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)
find_package(Qt6 COMPONENTS WaylandClientPrivate QUIET)
find_package(TreelandProtocols REQUIRED)

set(BIN_NAME test-super-overlay-surface)

qt_add_executable(${BIN_NAME}
    main.cpp
    ddeshellwayland.h ddeshellwayland.cpp
    ddeshelsurfacewindow.h ddeshelsurfacewindow.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::Widgets
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")


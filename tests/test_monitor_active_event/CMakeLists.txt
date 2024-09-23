find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)

set(BIN_NAME test-monitor-active_event)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${MODULE_PATH}/dde-shell/protocols/treeland-dde-shell-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::Widgets
        Qt6::WaylandClient
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")


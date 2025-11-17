find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)
find_package(TreelandProtocols REQUIRED)

set(BIN_NAME test-shortcut-manager)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-shortcut-manager-v2.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::Widgets
        Qt6::WaylandClient
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)

set(BIN_NAME test-window-bg)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${MODULE_PATH}/personalization/protocols/treeland-personalization-manager-v1.xml
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

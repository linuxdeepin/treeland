find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)

set(BIN_NAME test-virtual-output)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/virtual-output/protocols/treeland-virtual-output-manager-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Widgets
        Qt::WaylandClient
        Qt::GuiPrivate
        Qt::WaylandClientPrivate
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")


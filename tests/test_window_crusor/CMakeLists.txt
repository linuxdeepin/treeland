find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)

set(BIN_NAME test-window-cursor)

qt_add_executable(${BIN_NAME}
    main.cpp
    personalization_manager.h
    personalization_manager.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/personalization/protocols/treeland-personalization-manager-v1.xml
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

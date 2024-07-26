find_package(Qt6 REQUIRED COMPONENTS Core WaylandClient)

set(BIN_NAME test-wallpaper-color)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/wallpaper-color/protocols/treeland-wallpaper-color-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt${QT_MAJOR_VERSION}::Gui
        Qt::WaylandClient
        Qt::GuiPrivate
        Qt::WaylandClientPrivate
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

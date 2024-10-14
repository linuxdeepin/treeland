find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient)

set(BIN_NAME test-wallpaper-color)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${MODULE_PATH}/wallpaper-color/protocols/treeland-wallpaper-color-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::WaylandClient
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

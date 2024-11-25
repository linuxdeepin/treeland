find_package(Qt6 REQUIRED COMPONENTS WaylandClient Gui Widgets)
find_package(TreelandProtocols REQUIRED)

set(BIN_NAME test-window-cursor)

qt_add_executable(${BIN_NAME}
    main.cpp
    personalization_manager.h
    personalization_manager.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-personalization-manager-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::Widgets
        Qt6::WaylandClient
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

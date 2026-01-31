find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate WaylandClientPrivate)
endif()
find_package(TreelandProtocols REQUIRED)

set(BIN_NAME test-show-desktop)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-window-management-v1.xml
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

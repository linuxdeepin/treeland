find_package(Qt6 REQUIRED COMPONENTS WaylandClient Gui Widgets)
find_package(TreelandProtocols REQUIRED)

set(BIN_NAME test-window-overlapped)

qt_add_executable(${BIN_NAME}
    main.cpp
    dde_shell.h
    dde_shell.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::Widgets
        Qt6::WaylandClient
)

if(QT6_WAYLANDCLIENTPRIVATE_FOUND)
    target_link_libraries(${BIN_NAME}
        PRIVATE
            Qt6::WaylandClientPrivate
    )
endif()

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

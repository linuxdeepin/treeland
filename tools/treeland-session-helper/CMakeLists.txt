find_package(Qt6 REQUIRED COMPONENTS WaylandClient Gui Core DBus)

set(BIN_NAME treeland-session-helper)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${PROJECT_SOURCE_DIR}/src/protocols/treeland-app-id-resolver-unstable-v2.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::WaylandClient
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

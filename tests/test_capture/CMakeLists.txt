set(BIN_NAME test-capture)

find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient)
find_package(TreelandProtocols REQUIRED)

qt_add_executable(${BIN_NAME}
    main.cpp
    capture.h
    capture.cpp
)

qt6_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-capture-unstable-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

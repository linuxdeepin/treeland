find_package(Qt6 REQUIRED COMPONENTS WaylandClient Quick)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS WaylandClientPrivate)
endif()
find_package(TreelandProtocols REQUIRED)

set(BIN_NAME test-multitaskview)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_add_qml_module(${BIN_NAME}
    URI test.multitaskview
    VERSION 1.0
    QML_FILES
        Main.qml
    SOURCES
        ddeshell.h
        ddeshell.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    NO_INCLUDE_CORE_ONLY
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Quick
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

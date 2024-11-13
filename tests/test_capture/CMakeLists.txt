set(BIN_NAME test-capture)

find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient Quick)
find_package(TreelandProtocols REQUIRED)
list(APPEND QML_IMPORT_PATH ${CMAKE_CURRENT_BINARY_DIR})
qt_add_executable(${BIN_NAME}
    main.cpp
    capture.h
    capture.cpp
    canvaswindow.cpp
    canvaswindow.h
)
qt_add_qml_module(${BIN_NAME}
    URI capture
    VERSION 2.0
    QML_FILES
    Main.qml
)

qt_add_resources(${BIN_NAME} "test_capture_assets"
    PREFIX "/"
    BASE ${CMAKE_CURRENT_LIST_DIR}/images
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/images/watermark.png
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
        Qt6::Quick
        Qt6::QuickPrivate
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

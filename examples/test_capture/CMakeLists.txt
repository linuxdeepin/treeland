find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient Quick)
find_package(TreelandProtocols REQUIRED)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl gl)
qt_add_executable(test-capture
    main.cpp
)
qt_add_qml_module(test-capture
    URI capture
    VERSION 2.0
    SOURCES
    capture.h
    capture.cpp
    subwindow.h
    subwindow.cpp
    player.h
    player.cpp
    QML_FILES
    Main.qml
)

qt_add_resources(test-capture "test_capture_assets"
    PREFIX "/"
    BASE ${CMAKE_CURRENT_LIST_DIR}/images
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/images/watermark.png
)

qt6_generate_wayland_protocol_client_sources(test-capture
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-capture-unstable-v1.xml
)

target_link_libraries(test-capture
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
        Qt6::Quick
        Qt6::QuickPrivate
        Qt6::QuickControls2
        PkgConfig::EGL
)

install(TARGETS test-capture RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

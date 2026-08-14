find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient Quick)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS WaylandClientPrivate)
endif()
find_package(TreelandProtocols REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

set(BIN_NAME test-snap-capture)

qt_add_executable(${BIN_NAME}
    main.cpp
    screencapture.cpp
    snapproxy.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml
        ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml
        ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-capture-snap-unstable-v1.xml
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-toplevel-tag/xdg-toplevel-tag-v1.xml
)

# xdg-toplevel-tag references xdg_toplevel_interface from xdg-shell
qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
)

qt_add_qml_module(${BIN_NAME}
    URI TestSnapCapture
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
        Qt6::Quick
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

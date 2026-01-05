find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)
find_package(TreelandProtocols REQUIRED)

qt_add_executable(test-set-xwindow-position
    main.cpp
)

qt_generate_wayland_protocol_client_sources(test-set-xwindow-position
    FILES
    ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml
    NO_INCLUDE_CORE_ONLY
)

target_link_libraries(test-set-xwindow-position
    PRIVATE
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Widgets
    Qt6::WaylandClient
)

install(TARGETS test-set-xwindow-position RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

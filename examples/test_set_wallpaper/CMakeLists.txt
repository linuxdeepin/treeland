find_package(Qt6 COMPONENTS REQUIRED Gui DBus WaylandClient Widgets)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
  find_package(Qt6 COMPONENTS GuiPrivate WaylandClientPrivate REQUIRED)
endif()

find_package(TreelandProtocols REQUIRED)

set(BIN_NAME test_set_wallpaper)

qt_add_executable(${BIN_NAME}
    main.cpp
    treelandwallpapermanagerclient.h
    treelandwallpapermanagerclient.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-manager-unstable-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::Widgets
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")


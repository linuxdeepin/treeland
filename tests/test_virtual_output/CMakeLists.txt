find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)

qt_add_executable(test_virtual_output
    main.cpp
)

qt_generate_wayland_protocol_client_sources(test_virtual_output
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/virtual-output/protocols/treeland-virtual-output-manager-v1.xml
)

target_link_libraries(test_virtual_output
    PRIVATE
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Widgets
        Qt::WaylandClient
        Qt::GuiPrivate
        Qt::WaylandClientPrivate
)

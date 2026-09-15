find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_xwindow-control main.cpp)

target_link_libraries(test_protocol_xwindow-control
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_xwindow-control COMMAND test_protocol_xwindow-control)

set_property(TEST test_protocol_xwindow-control PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_xwindow-control PROPERTY
    TIMEOUT 3
)

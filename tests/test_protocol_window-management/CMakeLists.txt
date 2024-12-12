find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_window-management main.cpp)

target_link_libraries(test_protocol_window-management
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_window-management COMMAND test_protocol_window-management)

set_property(TEST test_protocol_window-management PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

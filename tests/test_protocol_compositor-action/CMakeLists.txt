find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_compositor-action main.cpp)

target_link_libraries(test_protocol_compositor-action
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_compositor-action COMMAND test_protocol_compositor-action)

set_property(TEST test_protocol_compositor-action PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_compositor-action PROPERTY
    TIMEOUT 3
)

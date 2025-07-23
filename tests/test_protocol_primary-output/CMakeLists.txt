find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_primary-output main.cpp)

target_link_libraries(test_protocol_primary-output
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_primary-output COMMAND test_protocol_primary-output)

set_property(TEST test_protocol_primary-output PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_primary-output PROPERTY
    TIMEOUT 3
)

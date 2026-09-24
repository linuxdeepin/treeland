find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_background-effect main.cpp)

target_link_libraries(test_protocol_background-effect
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_background-effect COMMAND test_protocol_background-effect)

set_property(TEST test_protocol_background-effect PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_background-effect PROPERTY
    TIMEOUT 3
)

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_personalization main.cpp)

target_link_libraries(test_protocol_personalization
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_personalization COMMAND test_protocol_personalization)

set_property(TEST test_protocol_personalization PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_personalization PROPERTY
    TIMEOUT 3
)

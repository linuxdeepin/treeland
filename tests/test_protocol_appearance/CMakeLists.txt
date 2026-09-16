find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_appearance main.cpp)

target_link_libraries(test_protocol_appearance
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_appearance COMMAND test_protocol_appearance)

set_property(TEST test_protocol_appearance PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_appearance PROPERTY
    TIMEOUT 3
)

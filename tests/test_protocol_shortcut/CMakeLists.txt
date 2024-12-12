find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_shortcut main.cpp)

target_link_libraries(test_protocol_shortcut
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_shortcut COMMAND test_protocol_shortcut)

set_property(TEST test_protocol_shortcut PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

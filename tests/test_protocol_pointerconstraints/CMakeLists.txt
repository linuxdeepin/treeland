find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_pointerconstraints main.cpp)

target_link_libraries(test_protocol_pointerconstraints
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_pointerconstraints COMMAND test_protocol_pointerconstraints)

set_property(TEST test_protocol_pointerconstraints PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_pointerconstraints PROPERTY
    TIMEOUT 3
)

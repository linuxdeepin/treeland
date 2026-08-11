find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_shortcutcontroller main.cpp)

target_link_libraries(test_shortcutcontroller
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_shortcutcontroller COMMAND test_shortcutcontroller)

set_property(TEST test_shortcutcontroller PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_shortcutcontroller PROPERTY
    TIMEOUT 10
)

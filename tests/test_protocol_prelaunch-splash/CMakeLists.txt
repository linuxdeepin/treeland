find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_prelaunch-splash main.cpp)

target_link_libraries(test_protocol_prelaunch-splash
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_prelaunch-splash COMMAND test_protocol_prelaunch-splash)

set_property(TEST test_protocol_prelaunch-splash PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_prelaunch-splash PROPERTY
    TIMEOUT 3
)

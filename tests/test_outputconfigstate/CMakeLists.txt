find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_outputconfigstate main.cpp)

target_link_libraries(test_outputconfigstate
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_outputconfigstate COMMAND test_outputconfigstate)

set_property(TEST test_outputconfigstate PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_outputconfigstate PROPERTY
    TIMEOUT 3
)

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_windowconfigstore main.cpp)
target_link_libraries(test_windowconfigstore PRIVATE libtreeland Qt6::Test)
add_test(NAME test_windowconfigstore COMMAND test_windowconfigstore)
set_tests_properties(test_windowconfigstore PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 10
)

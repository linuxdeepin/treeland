find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_greeterproxy main.cpp)
target_link_libraries(test_greeterproxy PRIVATE libtreeland Qt6::Test)
add_test(NAME test_greeterproxy COMMAND test_greeterproxy)
set_tests_properties(test_greeterproxy PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 10
)

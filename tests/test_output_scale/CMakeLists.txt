find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_output_scale main.cpp)
target_link_libraries(test_output_scale PRIVATE libtreeland Qt6::Test)
add_test(NAME test_output_scale COMMAND test_output_scale)
set_tests_properties(test_output_scale PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 10
)

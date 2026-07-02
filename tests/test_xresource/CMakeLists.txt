find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_xresource main.cpp)
target_link_libraries(test_xresource PRIVATE libtreeland Qt6::Test)
add_test(NAME test_xresource COMMAND test_xresource)
set_tests_properties(test_xresource PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 10
)

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_backlight main.cpp)
target_link_libraries(test_backlight PRIVATE libtreeland Qt6::Test)
add_test(NAME test_backlight COMMAND test_backlight)
set_tests_properties(test_backlight PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 10
)

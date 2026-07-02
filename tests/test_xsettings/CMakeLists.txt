find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_xsettings main.cpp)
target_link_libraries(test_xsettings PRIVATE libtreeland Qt6::Test)
add_test(NAME test_xsettings COMMAND test_xsettings)
set_tests_properties(test_xsettings PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 10
)

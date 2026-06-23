find_package(Qt6 REQUIRED COMPONENTS Test Quick)

add_executable(test_lockscreen main.cpp)
target_link_libraries(test_lockscreen PRIVATE libtreeland Qt6::Test Qt6::Quick)
add_test(NAME test_lockscreen COMMAND test_lockscreen)
set_tests_properties(test_lockscreen PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

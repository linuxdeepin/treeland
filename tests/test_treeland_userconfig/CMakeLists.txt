set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

# Test with VALID DConfig - reproduces real machine crash (SIGSEGV)
add_executable(test_dconfig_valid_config test_dconfig_valid_config.cpp)
target_link_libraries(test_dconfig_valid_config
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    libtreeland
)
target_include_directories(test_dconfig_valid_config PRIVATE
    ${CMAKE_BINARY_DIR}/src
)
add_test(NAME test_dconfig_valid_config COMMAND test_dconfig_valid_config)

# AI Review suggestions for CI stability
set_tests_properties(test_dconfig_valid_config PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 60
)

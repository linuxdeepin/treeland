set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

add_subdirectory(test_protocol_personalization)
add_subdirectory(test_protocol_primary-output)
add_subdirectory(test_protocol_shortcut)
add_subdirectory(test_protocol_virtual-output)
add_subdirectory(test_protocol_wallpaper-color)
add_subdirectory(test_protocol_window-management)
add_subdirectory(test_protocol_prelaunch-splash)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(test_dummy_ddm_greeter
    greeter/test_dummy_ddm_greeter.cpp
    ${CMAKE_SOURCE_DIR}/tools/treeland-ddm-debug/dummyddmstate.cpp
)

target_include_directories(test_dummy_ddm_greeter PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/treeland-ddm-debug
)

target_link_libraries(test_dummy_ddm_greeter
    PRIVATE
        Qt::Core
        Qt::Test
)

add_test(NAME test_dummy_ddm_greeter COMMAND test_dummy_ddm_greeter)

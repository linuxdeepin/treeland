# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
find_package(Qt6 6.8 REQUIRED COMPONENTS Test DBus)
find_package(Dtk6 REQUIRED COMPONENTS Core Tools)
find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
dtk_add_config_to_cpp(TEST_USER_CONFIG
    "${CMAKE_CURRENT_SOURCE_DIR}/test-config.json"
    OUTPUT_FILE_NAME "testconfig.hpp"
    CLASS_NAME "TestConfig")
qt_add_executable(test_user_config_backend
    main.cpp ${TEST_USER_CONFIG}
    "${PROJECT_SOURCE_DIR}/src/dconfig/userconfigbackend.cpp"
    "${PROJECT_SOURCE_DIR}/src/dconfig/userconfigbackend.h"
    "${PROJECT_SOURCE_DIR}/src/common/treelandlogging.cpp")
target_include_directories(test_user_config_backend PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(test_user_config_backend PRIVATE Qt6::Test Qt6::DBus Dtk6::Core)
add_test(NAME test_user_config_backend
    COMMAND "${DBUS_RUN_SESSION}" -- sh -c
    "DBUS_SYSTEM_BUS_ADDRESS=\"$DBUS_SESSION_BUS_ADDRESS\" exec \"$@\""
    sh "$<TARGET_FILE:test_user_config_backend>")
set_tests_properties(test_user_config_backend PROPERTIES TIMEOUT 30)

# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
find_package(Dtk6 REQUIRED COMPONENTS Core Tools)

dtk_add_config_to_cpp(TEST_LEVEL_GLOBAL_CONFIG
    "${PROJECT_SOURCE_DIR}/misc/dconfig/org.deepin.dde.treeland.json"
    OUTPUT_FILE_NAME "treelandconfig.hpp"
    CLASS_NAME "TreelandConfig")
dtk_add_config_to_cpp(TEST_LEVEL_USER_CONFIG
    "${PROJECT_SOURCE_DIR}/misc/dconfig/org.deepin.dde.treeland.user.json"
    OUTPUT_FILE_NAME "treelanduserconfig.hpp"
    CLASS_NAME "TreelandUserConfig")

qt_add_executable(test_effect_levels
    main.cpp
    ${TEST_LEVEL_GLOBAL_CONFIG}
    ${TEST_LEVEL_USER_CONFIG}
    "${PROJECT_SOURCE_DIR}/src/dconfig/effectlevel.h")

target_include_directories(test_effect_levels PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(test_effect_levels PRIVATE
    TREELAND_DCONFIG_DIR="${PROJECT_SOURCE_DIR}/misc/dconfig")
target_link_libraries(test_effect_levels PRIVATE Qt6::Test Dtk6::Core)

add_test(NAME test_effect_levels COMMAND "$<TARGET_FILE:test_effect_levels>")
set_tests_properties(test_effect_levels PROPERTIES TIMEOUT 30)

# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 REQUIRED COMPONENTS Test Qml Quick)

add_executable(test_wdamagegraph_demo
    tst_wdamagegraph_demo.cpp
    ../../manual/damage/demoscene.cpp
    ../../manual/damage/demoscene.h
    ../../manual/damage/damageoverlay.cpp
    ../../manual/damage/damageoverlay.h
    ../../manual/damage/visualnodemodel.cpp
    ../../manual/damage/visualnodemodel.h
)

target_include_directories(test_wdamagegraph_demo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../manual/damage"
)

target_link_libraries(test_wdamagegraph_demo
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
        Qt::Qml
        Qt::Quick
)

add_test(NAME test_wdamagegraph_demo COMMAND test_wdamagegraph_demo)

set_tests_properties(test_wdamagegraph_demo PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 60
)

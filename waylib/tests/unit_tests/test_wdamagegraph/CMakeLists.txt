# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 REQUIRED COMPONENTS Test Gui)

add_executable(test_wdamagegraph
    tst_wdamagegraph.cpp
)

target_link_libraries(test_wdamagegraph
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
        Qt::Gui
)

add_test(NAME test_wdamagegraph COMMAND test_wdamagegraph)

set_tests_properties(test_wdamagegraph PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 60
)

add_executable(bench_wdamagegraph
    bench_wdamagegraph.cpp
)

target_link_libraries(bench_wdamagegraph
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
        Qt::Gui
)

add_test(NAME bench_wdamagegraph COMMAND bench_wdamagegraph)

set_tests_properties(bench_wdamagegraph PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120
)

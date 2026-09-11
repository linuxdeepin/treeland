# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 REQUIRED COMPONENTS Test Quick)

add_executable(test_wsgdamage
    main.cpp
    scenetest.cpp
)

target_link_libraries(test_wsgdamage
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
        Qt::Quick
        Qt6::QuickPrivate
)

target_include_directories(test_wsgdamage PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/server/qtquick/scenegraph"
)

add_test(NAME test_wsgdamage COMMAND test_wsgdamage)

set_tests_properties(test_wsgdamage PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=rhi"
    TIMEOUT 120
)

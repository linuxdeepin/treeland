# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

# Unit tests for WSGDamageTracker — the damage-tracking algorithm is pure logic
# that operates on QSG node trees without a running compositor.

find_package(Qt6 REQUIRED COMPONENTS Test Quick)

add_executable(test_wsgdamagetracker main.cpp)

target_link_libraries(test_wsgdamagetracker
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
        Qt::Quick
        Qt6::QuickPrivate
)

add_test(NAME test_wsgdamagetracker COMMAND test_wsgdamagetracker)

set_property(TEST test_wsgdamagetracker PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

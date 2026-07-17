# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 REQUIRED COMPONENTS Test Gui Qml)

add_executable(test_wsgdamagetracker
    tst_wsgdamagetracker.cpp
)

target_link_libraries(test_wsgdamagetracker
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
        Qt::Gui
        Qt::Qml
)

add_test(NAME test_wsgdamagetracker COMMAND test_wsgdamagetracker)

set_property(TEST test_wsgdamagetracker PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

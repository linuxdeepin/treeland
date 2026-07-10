# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 REQUIRED COMPONENTS Test Quick)

add_executable(test_wsurfaceitem_lifetime main.cpp)

target_link_libraries(test_wsurfaceitem_lifetime
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
        Qt::Quick
        Qt6::QuickPrivate
)

add_test(NAME test_wsurfaceitem_lifetime COMMAND test_wsurfaceitem_lifetime)

set_property(TEST test_wsurfaceitem_lifetime PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

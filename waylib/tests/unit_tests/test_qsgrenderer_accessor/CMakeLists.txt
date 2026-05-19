# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 REQUIRED COMPONENTS Test Quick)

add_executable(test_qsgrenderer_accessor
    main.cpp
    qsgrenderer_direct.cpp
)

target_link_libraries(test_qsgrenderer_accessor
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
        Qt::Quick
        Qt6::QuickPrivate
)

add_test(NAME test_qsgrenderer_accessor COMMAND test_qsgrenderer_accessor)

set_property(TEST test_qsgrenderer_accessor PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

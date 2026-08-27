# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 REQUIRED COMPONENTS Test Quick)
find_package(X11)


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
if (X11_FOUND)
    target_link_libraries(test_wsgdamage PRIVATE X11::X11)
    target_compile_definitions(test_wsgdamage PRIVATE TEST_HAS_X11)
endif()

target_include_directories(test_wsgdamage PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/server/qtquick/scenegraph"
)

add_test(NAME test_wsgdamage COMMAND test_wsgdamage)

set_tests_properties(test_wsgdamage PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=rhi"
    TIMEOUT 120
)

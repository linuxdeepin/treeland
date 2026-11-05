# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_program(QML_TEST_RUNNER NAMES qmltestrunner HINTS /usr/lib/qt6/bin)
if(QML_TEST_RUNNER)
    add_test(NAME test_animation_switches
        COMMAND "${QML_TEST_RUNNER}" -input "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(test_animation_switches PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Basic;QT_QUICK_BACKEND=software;QML_XHR_ALLOW_FILE_READ=1"
        TIMEOUT 30)
endif()

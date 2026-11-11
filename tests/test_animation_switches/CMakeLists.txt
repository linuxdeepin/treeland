# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_program(QML_TEST_RUNNER NAMES qmltestrunner HINTS /usr/lib/qt6/bin)
if(QML_TEST_RUNNER)
    set(TEST_IMPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/imports/Treeland")
    configure_file(imports/Treeland/qmldir "${TEST_IMPORT_DIR}/qmldir" COPYONLY)
    configure_file(imports/Treeland/WaylandServer.qml "${TEST_IMPORT_DIR}/WaylandServer.qml" COPYONLY)
    configure_file("${PROJECT_SOURCE_DIR}/src/core/qml/EdgeTilePreview.qml"
                   "${TEST_IMPORT_DIR}/EdgeTilePreview.qml" COPYONLY)
    add_test(NAME test_animation_switches
        COMMAND "${QML_TEST_RUNNER}" -input "${CMAKE_CURRENT_SOURCE_DIR}"
                -import "${CMAKE_CURRENT_BINARY_DIR}/imports")
    set_tests_properties(test_animation_switches PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Basic;QT_QUICK_BACKEND=software"
        TIMEOUT 30)
endif()

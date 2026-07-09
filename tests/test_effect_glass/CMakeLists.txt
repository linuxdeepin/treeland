# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

# Liquid Glass effect test — uses waylib's headless backend for a real
# OpenGL context (via Mesa llvmpipe) without a display server or xvfb-run.
# The TestGlass QML module bundles GlassEffect.qml + shaders; TestWindow.qml
# wraps the test scene in an OutputRenderWindow + OutputItem so it renders
# through waylib's scene graph.

find_package(Qt6 REQUIRED COMPONENTS Test Core Qml Quick QuickControls2 ShaderTools)

qt_standard_project_setup(REQUIRES 6.4)

if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()
if(POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)

set(GLASS_SHADER_DIR "${CMAKE_SOURCE_DIR}/misc/shaders")
set(GLASS_EFFECT_QML "${CMAKE_SOURCE_DIR}/src/core/qml/Effects/GlassEffect.qml")
set_source_files_properties(${GLASS_EFFECT_QML}
    PROPERTIES
        QT_RESOURCE_ALIAS "GlassEffect.qml"
)

add_executable(test_effect_glass
    main.cpp
    TestHelper.cpp
)

qt_add_qml_module(test_effect_glass
    URI TestGlass
    VERSION "1.0"
    QML_FILES
        ${GLASS_EFFECT_QML}
        GlassEffectScene.qml
        TestWindow.qml
    SOURCES
        TestHelper.h
)

qt_add_shaders(test_effect_glass "test_glass_shaders"
    PRECOMPILE
    PREFIX
        "/shaders"
    BASE
        ${GLASS_SHADER_DIR}
    FILES
        ${GLASS_SHADER_DIR}/liquidglass.vert
        ${GLASS_SHADER_DIR}/liquidglass.frag
)

target_compile_definitions(test_effect_glass
    PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        WLR_USE_UNSTABLE
)

target_link_libraries(test_effect_glass
    PRIVATE
        Qt::Core
        Qt::Qml
        Qt::Quick
        Qt::QuickControls2
        Qt::Test
        waylibserver
        PkgConfig::PIXMAN
        PkgConfig::WAYLAND
)

# waylib QML imports are generated in the build tree.
set_target_properties(test_effect_glass PROPERTIES
    QT_QML_IMPORT_PATH "${PROJECT_BINARY_DIR}/waylib/src/server"
)

add_test(NAME test_effect_glass COMMAND test_effect_glass)
set_tests_properties(test_effect_glass PROPERTIES
    ENVIRONMENT "WLR_BACKENDS=headless;QT_QML_IMPORT_PATH=${PROJECT_BINARY_DIR}/waylib/src/server"
    TIMEOUT 60
)

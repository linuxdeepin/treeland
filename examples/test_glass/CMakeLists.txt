# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 COMPONENTS Quick QuickControls2 ShaderTools REQUIRED)
qt_standard_project_setup(REQUIRES 6.4)

if(QT_KNOWN_POLICY_QTP0001) # this policy was introduced in Qt 6.5
    qt_policy(SET QTP0001 NEW)
    # the RESOURCE_PREFIX argument for qt_add_qml_module() defaults to ":/qt/qml/"
endif()
if(POLICY CMP0071)
    # https://cmake.org/cmake/help/latest/policy/CMP0071.html
    cmake_policy(SET CMP0071 NEW)
endif()

set(QML_IMPORT_PATH "${PROJECT_BINARY_DIR}/src/server;${QML_IMPORT_PATH}" CACHE STRING "For LSP" FORCE)

find_package(PkgConfig REQUIRED)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)

set(GLASS_LIQUIDGLASS_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../misc/shaders")
set(GLASS_EFFECT_QML "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/qml/Effects/GlassEffect.qml")
set(GLASS_BLUR_QML "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/qml/Effects/Blur.qml")
set(LOCKSCREEN_ROUNDBLUR_QML "${CMAKE_CURRENT_SOURCE_DIR}/../../src/plugins/lockscreen/qml/RoundBlur.qml")
set_source_files_properties(${GLASS_EFFECT_QML}
    PROPERTIES
        QT_RESOURCE_ALIAS "GlassEffect.qml"
)
set_source_files_properties(${GLASS_BLUR_QML}
    PROPERTIES
        QT_RESOURCE_ALIAS "Blur.qml"
)
set_source_files_properties(${LOCKSCREEN_ROUNDBLUR_QML}
    PROPERTIES
        QT_RESOURCE_ALIAS "RoundBlur.qml"
)

# ── Stub Treeland QML module ─────────────────────────────────────────────
# Provides a minimal Helper singleton with default-valued glass config so
# the upstream Blur.qml / RoundBlur.qml — which read `Helper.config.*` and
# `import Treeland` — work inside the standalone demo without libtreeland.
qt_add_library(treeland_stub SHARED)
qt_add_qml_module(treeland_stub
    URI Treeland
    VERSION "2.0"
    QML_FILES
        ${GLASS_EFFECT_QML}
        ${GLASS_BLUR_QML}
        ${LOCKSCREEN_ROUNDBLUR_QML}
    SOURCES
        helper.h
        helper.cpp
)
qt_add_shaders(treeland_stub "treeland_stub_liquidglass_shaders"
    PRECOMPILE
    PREFIX
        "/shaders"
    BASE
        ${GLASS_LIQUIDGLASS_SHADER_DIR}
    FILES
        ${GLASS_LIQUIDGLASS_SHADER_DIR}/liquidglass.vert
        ${GLASS_LIQUIDGLASS_SHADER_DIR}/liquidglass.frag
)
target_compile_definitions(treeland_stub PRIVATE WLR_USE_UNSTABLE)
target_link_libraries(treeland_stub PUBLIC
    Qt6::Quick
    waylibserver
    PkgConfig::PIXMAN
    PkgConfig::WAYLAND
)

# ── test_glass executable ────────────────────────────────────────────────
add_executable(test_glass
    main.cpp
)
qt_add_qml_module(test_glass
    URI GlassExample
    VERSION "1.0"
    QML_FILES
        Main.qml
    RESOURCES
        assets/default-glass-background.jpg
)
target_compile_definitions(test_glass
    PRIVATE
    SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    $<$<BOOL:${START_DEMO}>:START_DEMO>
    WLR_USE_UNSTABLE
)
target_link_libraries(test_glass
    PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    treeland_stub
    PkgConfig::PIXMAN
    PkgConfig::WAYLAND
)
install(TARGETS test_glass DESTINATION ${CMAKE_INSTALL_BINDIR})

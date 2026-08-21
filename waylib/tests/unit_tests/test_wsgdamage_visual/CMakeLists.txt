# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

# DamageDebug overlay visual test: headless wlroots output, dump the
# committed wlr_buffer to QImage, grab the QML scene without overlay, paint
# the same highlight from recent damage entries, and compare.

find_package(Qt6 REQUIRED COMPONENTS Test Core Concurrent Qml Quick QuickControls2)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate QuickPrivate)
endif()
find_package(PkgConfig REQUIRED)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER_BIN wayland-scanner wayland_scanner)
if(NOT WAYLAND_SCANNER_BIN)
    find_program(WAYLAND_SCANNER_BIN NAMES wayland-scanner REQUIRED)
endif()

if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()
if(POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()

set(XDG_SHELL_XML "${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml")
set(XDG_SHELL_CLIENT_H "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
set(XDG_SHELL_CLIENT_C "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.c")
add_custom_command(
    OUTPUT ${XDG_SHELL_CLIENT_H} ${XDG_SHELL_CLIENT_C}
    COMMAND ${WAYLAND_SCANNER_BIN} client-header ${XDG_SHELL_XML} ${XDG_SHELL_CLIENT_H}
    COMMAND ${WAYLAND_SCANNER_BIN} private-code ${XDG_SHELL_XML} ${XDG_SHELL_CLIENT_C}
    DEPENDS ${XDG_SHELL_XML}
    COMMENT "Generating xdg-shell client protocol for test_wsgdamage_visual"
    VERBATIM
)

add_executable(test_wsgdamage_visual
    main.cpp
    helper.cpp
    damageclient.c
    ${XDG_SHELL_CLIENT_C}
)
set_source_files_properties(damageclient.c ${XDG_SHELL_CLIENT_C} PROPERTIES
    LANGUAGE C
    SKIP_AUTOMOC ON
    SKIP_AUTOUIC ON
)

# Reuse the glass example photo instead of adding a second wallpaper.
set(WSG_DAMAGE_WALLPAPER
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../examples/test_glass/assets/default-glass-background.jpg")
if(NOT EXISTS "${WSG_DAMAGE_WALLPAPER}")
    message(FATAL_ERROR
        "test_wsgdamage_visual needs examples/test_glass/assets/default-glass-background.jpg")
endif()
set_source_files_properties("${WSG_DAMAGE_WALLPAPER}"
    PROPERTIES QT_RESOURCE_ALIAS "default-glass-background.jpg")

qt_add_qml_module(test_wsgdamage_visual
    URI WSGDamageVisual
    VERSION "1.0"
    QML_FILES
        TestWindow.qml
        DamageScene.qml
    RESOURCES
        "${WSG_DAMAGE_WALLPAPER}"
    SOURCES
        helper.h
    IMPORTS
        Waylib.Server
)

target_compile_definitions(test_wsgdamage_visual
    PRIVATE
        WLR_USE_UNSTABLE
        WAYLIB_QML_IMPORT_PATH="${PROJECT_BINARY_DIR}/src/server"
)

target_include_directories(test_wsgdamage_visual PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/server/qtquick"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/server/qtquick/private"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/server/qtquick/scenegraph"
)

target_link_libraries(test_wsgdamage_visual
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
        Qt::Concurrent
        Qt::Qml
        Qt::Quick
        Qt::QuickControls2
        Qt6::QuickPrivate
        PkgConfig::PIXMAN
        PkgConfig::WAYLAND
        PkgConfig::WAYLAND_CLIENT
)

add_test(NAME test_wsgdamage_visual COMMAND test_wsgdamage_visual)
set_tests_properties(test_wsgdamage_visual PROPERTIES
    ENVIRONMENT "WLR_BACKENDS=headless;QT_QML_IMPORT_PATH=${PROJECT_BINARY_DIR}/src/server;QT_QUICK_CONTROLS_STYLE=Basic"
    TIMEOUT 120
)

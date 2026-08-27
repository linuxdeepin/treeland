# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 COMPONENTS Quick QuickControls2 Qml REQUIRED)
qt_standard_project_setup(REQUIRES 6.4)

if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()
if(POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()

set(DAMAGEGRAPH_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../damage")

qt_add_executable(damagegraph-demo
    main.cpp
)

qt_add_qml_module(damagegraph-demo
    URI DamageGraphDemo
    VERSION 1.0
    QML_FILES
        Main.qml
    SOURCES
        ${DAMAGEGRAPH_CORE}/demoscene.h
        ${DAMAGEGRAPH_CORE}/demoscene.cpp
        ${DAMAGEGRAPH_CORE}/damageoverlay.h
        ${DAMAGEGRAPH_CORE}/damageoverlay.cpp
        ${DAMAGEGRAPH_CORE}/visualnodemodel.h
        ${DAMAGEGRAPH_CORE}/visualnodemodel.cpp
)

target_include_directories(damagegraph-demo PRIVATE
    ${DAMAGEGRAPH_CORE}
)

target_link_libraries(damagegraph-demo PRIVATE
    waylibserver
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::Gui
)

# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 REQUIRED COMPONENTS Gui Qml Test)

add_executable(test_native_lifecycle main.cpp)

target_link_libraries(test_native_lifecycle
    PRIVATE
        Waylib::WaylibServer
        Qt::Gui
        Qt::Qml
        Qt::Test
        PkgConfig::WAYLAND_SERVER
)

add_test(NAME test_native_lifecycle COMMAND test_native_lifecycle)

set_property(TEST test_native_lifecycle PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

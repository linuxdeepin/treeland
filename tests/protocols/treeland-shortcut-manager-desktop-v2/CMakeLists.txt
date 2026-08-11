# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_desktop_integration_test(
    NAME treeland_shortcut_manager_desktop_v2
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-shortcut-manager-v2.xml"
    EXTRA_XMLS "${CMAKE_SOURCE_DIR}/3rdparty/wlroots/protocol/virtual-keyboard-unstable-v1.xml"
    EXTRA_LIBRARIES PkgConfig::XKBCOMMON
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-shortcut-manager-desktop-v2.c"
)

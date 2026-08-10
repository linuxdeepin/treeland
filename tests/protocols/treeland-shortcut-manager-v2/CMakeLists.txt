# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_protocol_test(
    NAME treeland_shortcut_manager_v2
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-shortcut-manager-v2.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-shortcut-manager-v2.c"
)

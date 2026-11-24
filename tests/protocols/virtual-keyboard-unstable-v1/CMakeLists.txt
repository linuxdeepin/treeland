# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_protocol_test(
    NAME virtual_keyboard_unstable_v1
    XML "${WLROOTS_PROTOCOL_DIR}/virtual-keyboard-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/virtual-keyboard-unstable-v1.c"
)

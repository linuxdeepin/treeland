# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_desktop_integration_test(
    NAME treeland_dde_shell_multitask_desktop_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-dde-shell-multitask-desktop-v1.c"
)

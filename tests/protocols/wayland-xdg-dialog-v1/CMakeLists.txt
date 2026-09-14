# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
treeland_add_protocol_test(
    NAME wayland_xdg_dialog_v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-dialog/xdg-dialog-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wayland-xdg-dialog-v1.c"
)

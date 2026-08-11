# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_desktop_integration_test(
    NAME treeland_prelaunch_splash_desktop_v2
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-prelaunch-splash-v2.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-prelaunch-splash-desktop-v2.c"
)

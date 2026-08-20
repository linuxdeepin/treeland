# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_protocol_test(
    NAME wlr_data_control_unstable_v1
    XML "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-data-control-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wlr-data-control-unstable-v1.c"
)

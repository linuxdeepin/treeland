# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
treeland_add_protocol_test(
    NAME treeland_appearance_unstable_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-appearance-unstable-v1.xml"
    EXTRA_XMLS "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-appearance-manager-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-appearance-unstable-v1.c"
)

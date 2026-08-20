# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_protocol_test(
    NAME input_method_unstable_v2
    XML "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/input-method-unstable-v2.xml"
    EXTRA_XMLS
        "${PROJECT_SOURCE_DIR}/waylib/src/server/protocols/private/text-input-unstable-v2.xml"
        "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/virtual-keyboard-unstable-v1.xml"
    EXTRA_LIBRARIES PkgConfig::XKBCOMMON
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/input-method-unstable-v2.c"
)

# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_protocol_test(
    NAME wlr_screencopy_unstable_v1
    XML "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-screencopy-unstable-v1.xml"
    EXTRA_XMLS "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-layer-shell-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wlr-screencopy-unstable-v1.c"
)

# The generic headless backend creates a 1280x720 output by default.  This
# test owns its output fixture and requires the documented 1920x1080 size.
set_property(TEST test_wlr_screencopy_unstable_v1 APPEND PROPERTY
    ENVIRONMENT "WLR_HEADLESS_OUTPUTS=0")

# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
treeland_add_protocol_test(
    NAME ext_background_effect_v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-background-effect/ext-background-effect-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/ext-background-effect-v1.c"
)

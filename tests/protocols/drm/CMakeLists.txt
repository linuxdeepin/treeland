# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_protocol_test(
    NAME drm
    XML "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/drm.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/drm.c"
)

pkg_check_modules(GBM REQUIRED IMPORTED_TARGET gbm)
target_link_libraries(test_drm PRIVATE PkgConfig::GBM)

# wl_drm needs a renderer with a DRM device. The headless fixture may not have
# one, so the client returns 77 when wlroots cannot create the global.
set_tests_properties(test_drm PROPERTIES SKIP_RETURN_CODE 77)

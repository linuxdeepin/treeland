# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_protocol_test(
    NAME wlr_gamma_control_unstable_v1
    XML "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-gamma-control-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wlr-gamma-control-unstable-v1.c"
)

# A DRM backend is intentionally opt-in. The default protocol environment is
# headless and cannot advertise or apply a hardware gamma LUT.
if(TREELAND_ENABLE_GPU_PROTOCOL_TESTS)
    treeland_add_protocol_test(
        NAME wlr_gamma_control_unstable_v1_drm
        XML "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-gamma-control-unstable-v1.xml"
        SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
        CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wlr-gamma-control-unstable-v1.c"
    )
    target_compile_definitions(test_wlr_gamma_control_unstable_v1_drm
        PRIVATE TREELAND_PROTOCOL_EXPECT_DRM_GAMMA=1)
    set_tests_properties(test_wlr_gamma_control_unstable_v1_drm PROPERTIES
        ENVIRONMENT "WLR_BACKENDS=drm;WLR_RENDERER=gles2;TREELAND_TEST_PLUGINS_PATH=${TREELAND_PLUGINS_OUTPUT_PATH};DSG_DATA_DIRS=${TREELAND_PROTOCOL_TEST_DSG_DATA_DIRS};TREELAND_PROTOCOL_TEST_DSG_DIR=${TREELAND_PROTOCOL_TEST_DSG_DATA_DIR}"
        LABELS "protocols;gpu;drm"
        SKIP_RETURN_CODE 77
    )
endif()

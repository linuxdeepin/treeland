# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

set(WLR_EXPORT_DMABUF_XML
    "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-export-dmabuf-unstable-v1.xml")

# The standard headless CI renderer is pixman. It cannot put a DMA-BUF in the
# output commit, so this target exercises the real temporary-cancel path.
treeland_add_protocol_test(
    NAME wlr_export_dmabuf_unstable_v1
    XML "${WLR_EXPORT_DMABUF_XML}"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wlr-export-dmabuf-unstable-v1.c"
)

# GPU runners opt in at configure time. A missing usable DMA-BUF output is a
# skipped capability, not a successful substitute for frame/object/ready.
if(TREELAND_ENABLE_GPU_PROTOCOL_TESTS)
    pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)
    pkg_check_modules(GLES2 REQUIRED IMPORTED_TARGET glesv2)
    treeland_add_protocol_test(
        NAME wlr_export_dmabuf_unstable_v1_gpu
        XML "${WLR_EXPORT_DMABUF_XML}"
        SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
        CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wlr-export-dmabuf-unstable-v1.c"
    )
    target_compile_definitions(test_wlr_export_dmabuf_unstable_v1_gpu
        PRIVATE TREELAND_PROTOCOL_EXPECT_DMABUF_READY=1)
    target_link_libraries(test_wlr_export_dmabuf_unstable_v1_gpu PRIVATE
        PkgConfig::EGL
        PkgConfig::GLES2
    )
    set_tests_properties(test_wlr_export_dmabuf_unstable_v1_gpu PROPERTIES
        ENVIRONMENT "WLR_BACKENDS=headless;WLR_RENDERER=gles2;TREELAND_TEST_PLUGINS_PATH=${TREELAND_PLUGINS_OUTPUT_PATH};DSG_DATA_DIRS=${TREELAND_PROTOCOL_TEST_DSG_DATA_DIRS};TREELAND_PROTOCOL_TEST_DSG_DIR=${TREELAND_PROTOCOL_TEST_DSG_DATA_DIR}"
        LABELS "protocols;gpu"
        SKIP_RETURN_CODE 77
    )
endif()

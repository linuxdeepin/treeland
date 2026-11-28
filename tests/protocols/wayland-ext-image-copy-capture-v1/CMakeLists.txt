# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS QuickPrivate)
endif()

treeland_add_protocol_test(
    NAME wayland_ext_image_copy_capture_v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml"
    EXTRA_XMLS "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml"
               "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wayland-ext-image-copy-capture-v1.c"
)

treeland_add_protocol_test(
    NAME wayland_ext_toplevel_image_capture_v1
    XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml"
    EXTRA_XMLS "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml"
               "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
               "${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
               "${WLR_PROTOCOLS_DATADIR}/unstable/wlr-output-power-management-unstable-v1.xml"
               "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-ddm-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/toplevel-setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wayland-ext-toplevel-image-capture-v1.c"
)
target_compile_definitions(test_wayland_ext_toplevel_image_capture_v1 PRIVATE WLR_USE_UNSTABLE)
target_link_libraries(test_wayland_ext_toplevel_image_capture_v1 PRIVATE Qt6::QuickPrivate)
set_property(TEST test_wayland_ext_toplevel_image_capture_v1 APPEND PROPERTY
    ENVIRONMENT "WLR_HEADLESS_OUTPUTS=0")

if(TREELAND_ENABLE_GPU_PROTOCOL_TESTS)
    treeland_add_protocol_test(
        NAME wayland_ext_toplevel_image_capture_v1_gpu
        XML "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml"
        EXTRA_XMLS "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml"
                   "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
                   "${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
                   "${WLR_PROTOCOLS_DATADIR}/unstable/wlr-output-power-management-unstable-v1.xml"
                   "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-ddm-v1.xml"
        SETUP "${CMAKE_CURRENT_SOURCE_DIR}/toplevel-setup.cpp"
        CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/wayland-ext-toplevel-image-capture-v1.c"
    )
    target_compile_definitions(test_wayland_ext_toplevel_image_capture_v1_gpu
        PRIVATE WLR_USE_UNSTABLE TREELAND_CAPTURE_TEST_GPU)
    target_link_libraries(test_wayland_ext_toplevel_image_capture_v1_gpu PRIVATE Qt6::QuickPrivate)
    get_property(gpu_environment TEST test_wayland_ext_toplevel_image_capture_v1_gpu PROPERTY ENVIRONMENT)
    list(FILTER gpu_environment EXCLUDE REGEX "^WLR_RENDERER=")
    list(APPEND gpu_environment "WLR_HEADLESS_OUTPUTS=0" "WLR_RENDERER=gles2")
    set_property(TEST test_wayland_ext_toplevel_image_capture_v1_gpu PROPERTY
        ENVIRONMENT "${gpu_environment}")
    set_property(TEST test_wayland_ext_toplevel_image_capture_v1_gpu APPEND PROPERTY LABELS gpu)
endif()

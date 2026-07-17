# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#
# CMake build for vendored wlroots 0.19 renderer code.
# Builds a static library that exposes internal wlroots renderer APIs
# for use by the waylib sgrenderer BAL (Backend Abstraction Layer).

# === Option: enable/disable individual backends ===
option(WSG_VENDOR_ENABLE_GLES2 "Enable vendored gles2 renderer backend" ON)
option(WSG_VENDOR_ENABLE_VULKAN "Enable vendored vulkan renderer backend" ON)
option(WSG_VENDOR_ENABLE_PIXMAN "Enable vendored pixman renderer backend" ON)

set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# === Generate config.h ===
include(CheckIncludeFile)
include(CheckFunctionExists)

check_include_file(sys/eventfd.h HAVE_EVENTFD)

# GBM function check (only if gbm is found)
find_package(PkgConfig)
pkg_search_module(GBM QUIET IMPORTED_TARGET gbm)
if(GBM_FOUND)
    set(CMAKE_REQUIRED_LIBRARIES PkgConfig::GBM)
    check_function_exists(gbm_bo_get_fd_for_plane HAVE_GBM_BO_GET_FD_FOR_PLANE)
    set(CMAKE_REQUIRED_LIBRARIES)
endif()

configure_file(
    ${VENDOR_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    @ONLY
)

# === Source files ===
set(VENDOR_SOURCES
    # Top-level render infrastructure
    render/color.c
    render/color_fallback.c
    render/dmabuf.c
    render/drm_format_set.c
    render/drm_syncobj.c
    render/pass.c
    render/pixel_format.c
    render/swapchain.c
    render/wlr_renderer.c
    render/wlr_texture.c

    # Internal buffer type implementations (needed by wlr_texture.c)
    render/types/buffer_dmabuf.c
    render/types/buffer_readonly_data.c

    # Allocator
    render/allocator/allocator.c
    render/allocator/drm_dumb.c
    render/allocator/shm.c

    # Utility functions
    render/util/matrix.c
    render/util/time.c
    render/util/env.c
    render/util/shm.c
    render/util/rect_union.c

)

# === Conditional backends ===

if(WSG_VENDOR_ENABLE_GLES2)
    pkg_search_module(EGL REQUIRED IMPORTED_TARGET egl)
    pkg_search_module(GLESV2 REQUIRED IMPORTED_TARGET glesv2)
    list(APPEND VENDOR_SOURCES
        render/egl.c
        render/gles2/renderer.c
        render/gles2/texture.c
        render/gles2/pass.c
        render/gles2/pixel_format.c
        extensions/wsg_wlroots_egl.c
        extensions/wsg_wlroots_gles2.c
    )
    if(GBM_FOUND)
        list(APPEND VENDOR_SOURCES render/allocator/gbm.c)
    endif()
endif()

if(WSG_VENDOR_ENABLE_VULKAN)
    find_program(GLSLANG glslangValidator)
    if(NOT GLSLANG)
        message(FATAL_ERROR "glslangValidator not found but vulkan backend is enabled. "
            "Install glslang or set WSG_VENDOR_ENABLE_VULKAN=OFF.")
    endif()
endif()

if(WSG_VENDOR_ENABLE_VULKAN)
    pkg_search_module(VULKAN REQUIRED IMPORTED_TARGET vulkan)
    list(APPEND VENDOR_SOURCES
        render/vulkan/renderer.c
        render/vulkan/texture.c
        render/vulkan/pass.c
        render/vulkan/vulkan.c
        render/vulkan/util.c
        render/vulkan/pixel_format.c
        extensions/wsg_wlroots_vulkan.c
    )
    # Generate vulkan shader SPIR-V headers via glslang
    set(VK_SHADERS common.vert texture.frag quad.frag output.frag)
    set(VK_SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/render/vulkan/shaders)
    foreach(shader ${VK_SHADERS})
        set(output_header ${VK_SHADER_OUT_DIR}/${shader}.h)
        set(var_name "${shader}")
        string(REPLACE "." "_" var_name "${var_name}")
        string(REPLACE "/" "_" var_name "${var_name}")
        add_custom_command(
            OUTPUT ${output_header}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${VK_SHADER_OUT_DIR}
            COMMAND ${GLSLANG} -V
                ${VENDOR_DIR}/render/vulkan/shaders/${shader}
                -o ${output_header}
                --vn ${var_name}_data
            DEPENDS ${VENDOR_DIR}/render/vulkan/shaders/${shader}
            COMMENT "Compiling vulkan shader: ${shader}"
        )
        list(APPEND VENDOR_SOURCES ${output_header})
    endforeach()
endif()

if(WSG_VENDOR_ENABLE_PIXMAN)
    pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
    list(APPEND VENDOR_SOURCES
        render/pixman/renderer.c
        render/pixman/pass.c
        render/pixman/pixel_format.c
        extensions/wsg_wlroots_pixman.c
    )
endif()

# udmabuf allocator (Linux only)
check_include_file(linux/udmabuf.h HAVE_UDMABUF_H)
if(HAVE_UDMABUF_H)
    list(APPEND VENDOR_SOURCES render/allocator/udmabuf.c)
endif()

# dmabuf: use linux implementation on Linux, fallback otherwise
check_include_file(linux/dma-buf.h HAVE_LINUX_DMA_BUF_H)
if(HAVE_LINUX_DMA_BUF_H)
    list(APPEND VENDOR_SOURCES render/dmabuf_linux.c)
else()
    list(APPEND VENDOR_SOURCES render/dmabuf_fallback.c)
endif()

# === Build static library ===
add_library(waylib_sgrenderer_wlroots_vendor STATIC ${VENDOR_SOURCES})
add_library(Waylib::SGRendererWlrootsVendor ALIAS waylib_sgrenderer_wlroots_vendor)

target_compile_definitions(waylib_sgrenderer_wlroots_vendor PRIVATE
    WLR_USE_UNSTABLE
    WLR_PRIVATE=
    _GNU_SOURCE
)

if(WSG_VENDOR_ENABLE_GLES2)
    target_compile_definitions(waylib_sgrenderer_wlroots_vendor PRIVATE
        WLR_HAS_GLES2_RENDERER=1
    )
endif()

# gbm.c is only compiled when both gles2 is enabled and GBM is found, so the
# macro must match that condition — defining it without gbm.c causes a link
# error (allocator.c calls wlr_gbm_allocator_create() under this guard).
if(WSG_VENDOR_ENABLE_GLES2 AND GBM_FOUND)
    target_compile_definitions(waylib_sgrenderer_wlroots_vendor PRIVATE
        WLR_HAS_GBM_ALLOCATOR=1
    )
endif()

if(WSG_VENDOR_ENABLE_VULKAN)
    target_compile_definitions(waylib_sgrenderer_wlroots_vendor PRIVATE
        WLR_HAS_VULKAN_RENDERER=1
    )
endif()

if(HAVE_UDMABUF_H)
    target_compile_definitions(waylib_sgrenderer_wlroots_vendor PRIVATE
        WLR_HAS_UDMABUF_ALLOCATOR=1
    )
endif()

target_include_directories(waylib_sgrenderer_wlroots_vendor
    PUBLIC
        ${VENDOR_DIR}/include
        ${VENDOR_DIR}/include/render
        ${VENDOR_DIR}/extensions
    PRIVATE
        ${VENDOR_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${VENDOR_DIR}/render/gles2/shaders
)

# === Link dependencies ===
target_link_libraries(waylib_sgrenderer_wlroots_vendor
    PRIVATE
        PkgConfig::WLROOTS
        PkgConfig::LIBDRM
        PkgConfig::PIXMAN
        PkgConfig::WAYLAND
)

if(WSG_VENDOR_ENABLE_GLES2)
    target_link_libraries(waylib_sgrenderer_wlroots_vendor PRIVATE
        PkgConfig::EGL
        PkgConfig::GLESV2
    )
    if(GBM_FOUND)
        target_link_libraries(waylib_sgrenderer_wlroots_vendor PRIVATE PkgConfig::GBM)
    endif()
endif()

if(WSG_VENDOR_ENABLE_VULKAN)
    target_link_libraries(waylib_sgrenderer_wlroots_vendor PRIVATE
        PkgConfig::VULKAN
    )
endif()

set_target_properties(waylib_sgrenderer_wlroots_vendor PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 23
)

# Vendored third-party code: don't treat warnings as errors
# Force-include system pixman.h: some system wlroots headers use
# pixman_region32_t without including <pixman.h>. The vendored header was
# renamed to wlr_pixman.h to avoid collision with system <pixman.h>.
if(DEFINED PIXMAN_INCLUDE_DIRS)
    list(GET PIXMAN_INCLUDE_DIRS 0 _wsg_pixman_inc_dir)
    target_compile_options(waylib_sgrenderer_wlroots_vendor PRIVATE
        -Wno-error
        "-include${_wsg_pixman_inc_dir}/pixman.h"
    )
else()
    target_compile_options(waylib_sgrenderer_wlroots_vendor PRIVATE -Wno-error)
endif()

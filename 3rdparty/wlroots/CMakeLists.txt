# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: GPL-3.0-or-later

# Vendored wlroots 0.19.3 — CMake build that replaces the system
# pkg-config wlroots-0.19 module.  Built as an internal library:
# headers are exposed via include directories but NOT installed to
# the system prefix (issue requirement).  All public link
# dependencies are propagated so existing consumers (qwlroots,
# waylib, treeland) need zero source changes.

cmake_minimum_required(VERSION 3.25)

include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(TestBigEndian)

# ── Version ──────────────────────────────────────────────────────────────────
set(WLROOTS_VERSION      "0.19.3")
set(WLROOTS_VERSION_MAJOR 0)
set(WLROOTS_VERSION_MINOR 19)
set(WLROOTS_VERSION_PATCH 3)

# WLR_VERSION_* — consumed by cmake/wlr_version.h.in (aligns with meson
# include/wlr/meson.build; MICRO == patch component)
set(WLR_VERSION_STR   "0.19.3")
set(WLR_VERSION_MAJOR 0)
set(WLR_VERSION_MINOR 19)
set(WLR_VERSION_MICRO 3)

# ── Required dependencies ────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER  REQUIRED IMPORTED_TARGET "wayland-server>=1.23.1")
pkg_check_modules(WAYLAND_CLIENT  REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(LIBDRM          REQUIRED IMPORTED_TARGET "libdrm>=2.4.122")
pkg_check_modules(XKBCOMMON       REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(PIXMAN          REQUIRED IMPORTED_TARGET "pixman-1>=0.43.0")
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED "wayland-protocols>=1.41")
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

find_program(WAYLAND_SCANNER NAMES wayland-scanner REQUIRED)

# ── Feature options (meson "auto" equivalents) ───────────────────────────────
option(WLR_DRM_BACKEND       "DRM backend"        ON)
option(WLR_LIBINPUT_BACKEND  "libinput backend"   ON)
option(WLR_X11_BACKEND       "X11 backend"        ON)
option(WLR_XWAYLAND          "Xwayland"            ON)
option(WLR_GLES2_RENDERER    "GLES2 renderer"     ON)
option(WLR_VULKAN_RENDERER   "Vulkan renderer"    OFF)
option(WLR_GBM_ALLOCATOR     "GBM allocator"      ON)
option(WLR_UDMABUF_ALLOCATOR "udmabuf allocator"  ON)
option(WLR_SESSION           "Session support"    ON)
option(WLR_COLOR_MANAGEMENT  "Color management"   ON)
option(WLR_LIBLIFTOFF        "libliftoff"         ON)

# ── Feature detection ────────────────────────────────────────────────────────
set(WLR_HAS_SESSION          OFF)
set(WLR_HAS_DRM_BACKEND      OFF)
set(WLR_HAS_LIBINPUT_BACKEND OFF)
set(WLR_HAS_X11_BACKEND      OFF)
set(WLR_HAS_GLES2_RENDERER   OFF)
set(WLR_HAS_VULKAN_RENDERER  OFF)
set(WLR_HAS_GBM_ALLOCATOR    OFF)
set(WLR_HAS_UDMABUF_ALLOCATOR OFF)
set(WLR_HAS_XWAYLAND         OFF)
set(WLR_HAS_COLOR_MANAGEMENT OFF)
set(HAVE_EGL                 OFF)
set(HAVE_LIBLIFTOFF          OFF)
set(HAVE_LIBLIFTOFF_0_5      OFF)
set(HAVE_XCB_ERRORS          OFF)
set(HAVE_EVENTFD             OFF)
set(HAVE_GBM_BO_GET_FD_FOR_PLANE OFF)
set(HAVE_LIBINPUT_BUSTYPE    OFF)
set(HAVE_LIBINPUT_SWITCH_KEYPAD_SLIDE OFF)
set(HAVE_XCB_XFIXES_SET_CLIENT_DISCONNECT_MODE OFF)

# Session (libudev + libseat)
if(WLR_SESSION)
    pkg_check_modules(LIBUDEV IMPORTED_TARGET libudev)
    pkg_check_modules(LIBSEAT IMPORTED_TARGET "libseat>=0.2.0")
    if(LIBUDEV_FOUND AND LIBSEAT_FOUND)
        set(WLR_HAS_SESSION ON)
    endif()
endif()

# DRM backend (hwdata + libdisplay-info + session)
if(WLR_DRM_BACKEND AND WLR_HAS_SESSION)
    pkg_check_modules(HWDATA hwdata)
    pkg_check_modules(LIBDISPLAY_INFO IMPORTED_TARGET libdisplay-info)
    if(HWDATA_FOUND AND LIBDISPLAY_INFO_FOUND)
        set(WLR_HAS_DRM_BACKEND ON)
    endif()
endif()

# libliftoff (optional, for DRM backend)
if(WLR_LIBLIFTOFF AND WLR_HAS_DRM_BACKEND)
    pkg_check_modules(LIBLIFTOFF IMPORTED_TARGET "libliftoff>=0.4.0")
    if(LIBLIFTOFF_FOUND)
        set(HAVE_LIBLIFTOFF ON)
        if(LIBLIFTOFF_VERSION VERSION_GREATER_EQUAL "0.5.0")
            set(HAVE_LIBLIFTOFF_0_5 ON)
        endif()
    endif()
endif()

# libinput backend (libinput + session)
if(WLR_LIBINPUT_BACKEND AND WLR_HAS_SESSION)
    pkg_check_modules(LIBINPUT IMPORTED_TARGET "libinput>=1.19.0")
    if(LIBINPUT_FOUND)
        set(WLR_HAS_LIBINPUT_BACKEND ON)
        if(LIBINPUT_VERSION VERSION_GREATER_EQUAL "1.26.0")
            set(HAVE_LIBINPUT_BUSTYPE ON)
        endif()
        if(LIBINPUT_VERSION VERSION_GREATER_EQUAL "1.30.901")
            set(HAVE_LIBINPUT_SWITCH_KEYPAD_SLIDE ON)
        endif()
    endif()
endif()

# X11 backend
if(WLR_X11_BACKEND)
    pkg_check_modules(XCB_BACKEND IMPORTED_TARGET
        xcb xcb-dri3 xcb-present xcb-render xcb-renderutil xcb-shm xcb-xfixes xcb-xinput)
    if(XCB_BACKEND_FOUND)
        set(WLR_HAS_X11_BACKEND ON)
    endif()
endif()

# GBM (shared by gles2 renderer egl.c and gbm allocator)
pkg_check_modules(GBM IMPORTED_TARGET "gbm>=17.1.0")

# GBM allocator
if(WLR_GBM_ALLOCATOR AND GBM_FOUND)
    set(WLR_HAS_GBM_ALLOCATOR ON)
    set(_old_req ${CMAKE_REQUIRED_LIBRARIES})
    set(_old_inc ${CMAKE_REQUIRED_INCLUDES})
    set(CMAKE_REQUIRED_LIBRARIES ${GBM_LIBRARIES})
    set(CMAKE_REQUIRED_INCLUDES ${GBM_INCLUDE_DIRS})
    check_symbol_exists(gbm_bo_get_fd_for_plane "gbm.h" HAVE_GBM_BO_GET_FD_FOR_PLANE)
    set(CMAKE_REQUIRED_LIBRARIES ${_old_req})
    set(CMAKE_REQUIRED_INCLUDES ${_old_inc})
endif()

# GLES2 renderer (egl + glesv2, reuses GBM check above)
if(WLR_GLES2_RENDERER)
    pkg_check_modules(EGL IMPORTED_TARGET egl)
    pkg_check_modules(GLESV2 IMPORTED_TARGET glesv2)
    if(EGL_FOUND AND GBM_FOUND)
        set(HAVE_EGL ON)
        if(GLESV2_FOUND)
            set(WLR_HAS_GLES2_RENDERER ON)
        endif()
    endif()
endif()

# Vulkan renderer
if(WLR_VULKAN_RENDERER)
    pkg_check_modules(VULKAN IMPORTED_TARGET "vulkan>=1.2.182")
    find_program(GLSLANG_PROGRAM NAMES glslangValidator glslang)
    if(VULKAN_FOUND AND GLSLANG_PROGRAM)
        set(WLR_HAS_VULKAN_RENDERER ON)
    endif()
endif()

# udmabuf allocator
if(WLR_UDMABUF_ALLOCATOR)
    set(_old_def ${CMAKE_REQUIRED_DEFINITIONS})
    set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
    check_symbol_exists(memfd_create "sys/mman.h" _have_memfd)
    check_symbol_exists(F_ADD_SEALS "fcntl.h" _have_f_add_seals)
    check_include_file(linux/udmabuf.h _have_udmabuf_h)
    set(CMAKE_REQUIRED_DEFINITIONS ${_old_def})
    if(_have_memfd AND _have_f_add_seals AND _have_udmabuf_h)
        set(WLR_HAS_UDMABUF_ALLOCATOR ON)
    endif()
endif()

# Xwayland
set(XWAYLAND_PATH "")
if(WLR_XWAYLAND)
    pkg_check_modules(XWAYLAND xwayland)
    pkg_check_modules(XCB_XWAYLAND IMPORTED_TARGET
        xcb xcb-composite xcb-ewmh xcb-icccm xcb-render xcb-res xcb-xfixes)
    pkg_check_modules(XCB_ERRORS IMPORTED_TARGET xcb-errors)
    if(XWAYLAND_FOUND AND XCB_XWAYLAND_FOUND)
        set(WLR_HAS_XWAYLAND ON)
        pkg_get_variable(XWAYLAND_PATH xwayland xwayland)
        if(XCB_ERRORS_FOUND)
            set(HAVE_XCB_ERRORS ON)
        endif()
        foreach(_feat listenfd no_touch_pointer_emulation force_xrandr_emulation terminate_delay)
            string(TOUPPER ${_feat} _feat_upper)
            pkg_get_variable(_val xwayland have_${_feat})
            if(_val STREQUAL "true")
                set(HAVE_XWAYLAND_${_feat_upper} ON)
            endif()
        endforeach()
        set(_old_req ${CMAKE_REQUIRED_LIBRARIES})
        set(CMAKE_REQUIRED_LIBRARIES ${XCB_XWAYLAND_LIBRARIES})
        check_symbol_exists(xcb_xfixes_set_client_disconnect_mode "xcb/xfixes.h"
            HAVE_XCB_XFIXES_SET_CLIENT_DISCONNECT_MODE)
        set(CMAKE_REQUIRED_LIBRARIES ${_old_req})
    endif()
endif()

# Color management (lcms2)
if(WLR_COLOR_MANAGEMENT)
    pkg_check_modules(LCMS2 IMPORTED_TARGET lcms2)
    if(LCMS2_FOUND)
        set(WLR_HAS_COLOR_MANAGEMENT ON)
    endif()
endif()

# Misc header / function checks
check_include_file(sys/eventfd.h HAVE_EVENTFD)
check_include_file(linux/dma-buf.h HAVE_LINUX_DMA_BUF_H)

# Endianness
test_big_endian(WLR_IS_BIG_ENDIAN)
if(WLR_IS_BIG_ENDIAN)
    set(WLR_BIG_ENDIAN 1)
    set(WLR_LITTLE_ENDIAN 0)
else()
    set(WLR_BIG_ENDIAN 0)
    set(WLR_LITTLE_ENDIAN 1)
endif()

# ICONDIR
set(ICONDIR "${CMAKE_INSTALL_FULL_DATADIR}/icons")

# ── Generate config.h / version.h / internal config.h ────────────────────────
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/wlr_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/wlr/config.h @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/wlr_version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/wlr/version.h @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/internal_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

# ── Wayland protocol generation ──────────────────────────────────────────────
set(WLR_PROTOCOL_XMLS
    # Stable upstream
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/linux-dmabuf/linux-dmabuf-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/presentation-time/presentation-time.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/tablet/tablet-v2.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/viewporter/viewporter.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
    # Staging upstream
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/alpha-modifier/alpha-modifier-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/color-management/color-management-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/content-type/content-type-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/drm-lease/drm-lease-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-idle-notify/ext-idle-notify-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-session-lock/ext-session-lock-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-data-control/ext-data-control-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/linux-drm-syncobj/linux-drm-syncobj-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/security-context/security-context-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/single-pixel-buffer/single-pixel-buffer-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-dialog/xdg-dialog-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-system-bell/xdg-system-bell-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/xwayland-shell/xwayland-shell-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/tearing-control/tearing-control-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-transient-seat/ext-transient-seat-v1.xml
    # Unstable upstream
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/primary-selection/primary-selection-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml
    # Repo-local protocols
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/drm.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/input-method-unstable-v2.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/server-decoration.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/virtual-keyboard-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-data-control-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-export-dmabuf-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-foreign-toplevel-management-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-gamma-control-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-layer-shell-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-output-management-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-output-power-management-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-screencopy-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-virtual-pointer-unstable-v1.xml
)

set(WLR_PROTOCOL_SOURCES "")
foreach(_xml ${WLR_PROTOCOL_XMLS})
    get_filename_component(_base ${_xml} NAME_WE)
    set(_c ${CMAKE_CURRENT_BINARY_DIR}/${_base}-protocol.c)
    set(_h ${CMAKE_CURRENT_BINARY_DIR}/${_base}-protocol.h)
    add_custom_command(OUTPUT ${_c} ${_h}
        COMMAND ${WAYLAND_SCANNER} private-code  ${_xml} ${_c}
        COMMAND ${WAYLAND_SCANNER} server-header ${_xml} ${_h}
        DEPENDS ${_xml}
        COMMENT "wayland-scanner: ${_base}")
    list(APPEND WLR_PROTOCOL_SOURCES ${_c} ${_h})
endforeach()

# Client protocol headers (used by backend/wayland)
set(WLR_CLIENT_PROTOCOL_XMLS
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol/drm.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/linux-dmabuf/linux-dmabuf-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/linux-drm-syncobj/linux-drm-syncobj-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/presentation-time/presentation-time.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/tablet/tablet-v2.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/viewporter/viewporter.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
)
foreach(_xml ${WLR_CLIENT_PROTOCOL_XMLS})
    get_filename_component(_base ${_xml} NAME_WE)
    set(_h ${CMAKE_CURRENT_BINARY_DIR}/${_base}-client-protocol.h)
    add_custom_command(OUTPUT ${_h}
        COMMAND ${WAYLAND_SCANNER} client-header ${_xml} ${_h}
        DEPENDS ${_xml}
        COMMENT "wayland-scanner (client): ${_base}")
    list(APPEND WLR_PROTOCOL_SOURCES ${_h})
endforeach()

# ── GLES2 shader embedding ───────────────────────────────────────────────────
# Inline hex embedding — no subprocess, no external script dependency.
# file(READ ... HEX) requires CMake >= 3.27; CI runs 3.31+.
set(WLR_GLES2_SHADER_FILES "")
if(WLR_HAS_GLES2_RENDERER)
    foreach(_shader common.vert quad.frag tex_rgba.frag tex_rgbx.frag tex_external.frag)
        string(REPLACE "." "_" _base ${_shader})
        set(_var  "${_base}_src")
        set(_in   ${CMAKE_CURRENT_SOURCE_DIR}/render/gles2/shaders/${_shader})
        set(_out  ${CMAKE_CURRENT_BINARY_DIR}/${_var}.h)

        file(READ "${_in}" _hex HEX)
        string(LENGTH "${_hex}" _hex_len)
        math(EXPR _byte_count "${_hex_len} / 2")

        set(_content "static const char ${_var}[] = {\n")
        set(_col 0)
        math(EXPR _last_idx "${_byte_count} - 1")
        foreach(_i RANGE 0 ${_last_idx})
            math(EXPR _off "${_i} * 2")
            string(SUBSTRING "${_hex}" "${_off}" 2 _byte)
            string(APPEND _content "    0x${_byte},")
            math(EXPR _col "${_col} + 1")
            if(_col EQUAL 12)
                string(APPEND _content "\n")
                set(_col 0)
            endif()
        endforeach()
        string(APPEND _content "\n    0x00,\n};\n")
        file(WRITE "${_out}" "${_content}")

        list(APPEND WLR_GLES2_SHADER_FILES ${_out})
    endforeach()
endif()

# ── Vulkan shader compilation ────────────────────────────────────────────────
set(WLR_VULKAN_SHADER_FILES "")
if(WLR_HAS_VULKAN_RENDERER)
    foreach(_shader common.vert texture.frag quad.frag output.frag)
        get_filename_component(_base ${_shader} NAME_WE)
        string(REPLACE "." "_" _name ${_shader})
        set(_in  ${CMAKE_CURRENT_SOURCE_DIR}/render/vulkan/shaders/${_shader})
        set(_out ${CMAKE_CURRENT_BINARY_DIR}/${_shader}.h)
        add_custom_command(OUTPUT ${_out}
            COMMAND ${GLSLANG_PROGRAM} -V ${_in} -o ${_out} --vn ${_name}_data --quiet
            DEPENDS ${_in}
            COMMENT "Compiling vulkan shader: ${_shader}" VERBATIM)
        list(APPEND WLR_VULKAN_SHADER_FILES ${_out})
    endforeach()
endif()

# ── pnpids.c generation (DRM backend) ────────────────────────────────────────
set(WLR_PNPIDS_SOURCE "")
if(WLR_HAS_DRM_BACKEND)
    pkg_get_variable(HWDATA_DATADIR hwdata pkgdatadir)
    set(WLR_PNPIDS_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/pnpids.c)
    execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/backend/drm/gen_pnpids.sh
        INPUT_FILE ${HWDATA_DATADIR}/pnp.ids
        OUTPUT_FILE ${WLR_PNPIDS_SOURCE} RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "Failed to generate pnpids.c")
    endif()
endif()

# ── Source file collection ───────────────────────────────────────────────────
set(WLR_SOURCES "")

# backend (base + always-on subdirs)
list(APPEND WLR_SOURCES backend/backend.c)
file(GLOB _srcs CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/multi/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/wayland/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/headless/*.c)
list(APPEND WLR_SOURCES ${_srcs})

if(WLR_HAS_SESSION)
    file(GLOB _srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/backend/session/*.c)
    list(APPEND WLR_SOURCES ${_srcs})
endif()

if(WLR_HAS_DRM_BACKEND)
    file(GLOB _srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/backend/drm/*.c)
    if(NOT HAVE_LIBLIFTOFF)
        list(REMOVE_ITEM _srcs ${CMAKE_CURRENT_SOURCE_DIR}/backend/drm/libliftoff.c)
    endif()
    list(APPEND WLR_SOURCES ${_srcs} ${WLR_PNPIDS_SOURCE})
endif()

if(WLR_HAS_LIBINPUT_BACKEND)
    file(GLOB _srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/backend/libinput/*.c)
    list(APPEND WLR_SOURCES ${_srcs})
endif()

if(WLR_HAS_X11_BACKEND)
    file(GLOB _srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/backend/x11/*.c)
    list(APPEND WLR_SOURCES ${_srcs})
endif()

# render (base)
list(APPEND WLR_SOURCES
    render/color.c
    render/dmabuf.c
    render/drm_format_set.c
    render/drm_syncobj.c
    render/pass.c
    render/pixel_format.c
    render/swapchain.c
    render/wlr_renderer.c
    render/wlr_texture.c)

if(HAVE_LINUX_DMA_BUF_H AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND WLR_SOURCES render/dmabuf_linux.c)
else()
    list(APPEND WLR_SOURCES render/dmabuf_fallback.c)
endif()

if(HAVE_EGL)
    list(APPEND WLR_SOURCES render/egl.c)
endif()

if(WLR_HAS_COLOR_MANAGEMENT)
    list(APPEND WLR_SOURCES render/color_lcms2.c)
else()
    list(APPEND WLR_SOURCES render/color_fallback.c)
endif()

file(GLOB _srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/render/pixman/*.c)
list(APPEND WLR_SOURCES ${_srcs})

list(APPEND WLR_SOURCES
    render/allocator/allocator.c
    render/allocator/shm.c
    render/allocator/drm_dumb.c)
if(WLR_HAS_GBM_ALLOCATOR)
    list(APPEND WLR_SOURCES render/allocator/gbm.c)
endif()
if(WLR_HAS_UDMABUF_ALLOCATOR)
    list(APPEND WLR_SOURCES render/allocator/udmabuf.c)
endif()

if(WLR_HAS_GLES2_RENDERER)
    file(GLOB _srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/render/gles2/*.c)
    list(APPEND WLR_SOURCES ${_srcs} ${WLR_GLES2_SHADER_FILES})
endif()

if(WLR_HAS_VULKAN_RENDERER)
    file(GLOB _srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/render/vulkan/*.c)
    list(APPEND WLR_SOURCES ${_srcs} ${WLR_VULKAN_SHADER_FILES})
endif()

# types
file(GLOB _srcs CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/types/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/types/*/*.c)
if(NOT WLR_HAS_DRM_BACKEND)
    list(REMOVE_ITEM _srcs ${CMAKE_CURRENT_SOURCE_DIR}/types/wlr_drm_lease_v1.c)
endif()
list(APPEND WLR_SOURCES ${_srcs})

# util + xcursor
file(GLOB _srcs CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/util/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/xcursor/*.c)
list(APPEND WLR_SOURCES ${_srcs})

# xwayland
if(WLR_HAS_XWAYLAND)
    file(GLOB _srcs CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/xwayland/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/xwayland/*/*.c)
    list(APPEND WLR_SOURCES ${_srcs})
endif()

# ── Library target ───────────────────────────────────────────────────────────
add_library(wlroots SHARED ${WLR_SOURCES} ${WLR_PROTOCOL_SOURCES})

set_target_properties(wlroots PROPERTIES
    OUTPUT_NAME waylib-wlroots
    VERSION   ${WLROOTS_VERSION}
    SOVERSION 0
    C_STANDARD 23
    POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(wlroots
    PUBLIC  WLR_USE_UNSTABLE
    PRIVATE WLR_PRIVATE=
            WLR_LITTLE_ENDIAN=${WLR_LITTLE_ENDIAN}
            WLR_BIG_ENDIAN=${WLR_BIG_ENDIAN}
            _POSIX_C_SOURCE=200809L)

target_compile_options(wlroots PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
    -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)

target_include_directories(wlroots PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

# ── Link libraries (all PUBLIC — matches system pkg-config propagation) ──────
set(WLR_LINK_LIBS
    PkgConfig::WAYLAND_SERVER
    PkgConfig::WAYLAND_CLIENT
    PkgConfig::LIBDRM
    PkgConfig::XKBCOMMON
    PkgConfig::PIXMAN
    m
    rt)

if(WLR_HAS_SESSION)
    list(APPEND WLR_LINK_LIBS PkgConfig::LIBUDEV PkgConfig::LIBSEAT)
endif()
if(WLR_HAS_DRM_BACKEND)
    list(APPEND WLR_LINK_LIBS PkgConfig::LIBDISPLAY_INFO)
    if(HAVE_LIBLIFTOFF)
        list(APPEND WLR_LINK_LIBS PkgConfig::LIBLIFTOFF)
    endif()
endif()
if(WLR_HAS_LIBINPUT_BACKEND)
    list(APPEND WLR_LINK_LIBS PkgConfig::LIBINPUT)
endif()
if(WLR_HAS_X11_BACKEND)
    list(APPEND WLR_LINK_LIBS PkgConfig::XCB_BACKEND)
endif()
if(HAVE_EGL)
    list(APPEND WLR_LINK_LIBS PkgConfig::EGL PkgConfig::GBM)
endif()
if(WLR_HAS_GLES2_RENDERER)
    list(APPEND WLR_LINK_LIBS PkgConfig::GLESV2)
endif()
if(WLR_HAS_GBM_ALLOCATOR AND NOT HAVE_EGL)
    list(APPEND WLR_LINK_LIBS PkgConfig::GBM)
endif()
if(WLR_HAS_VULKAN_RENDERER)
    list(APPEND WLR_LINK_LIBS PkgConfig::VULKAN)
endif()
if(WLR_HAS_XWAYLAND)
    list(APPEND WLR_LINK_LIBS PkgConfig::XCB_XWAYLAND)
    if(HAVE_XCB_ERRORS)
        list(APPEND WLR_LINK_LIBS PkgConfig::XCB_ERRORS)
    endif()
endif()
if(WLR_HAS_COLOR_MANAGEMENT)
    list(APPEND WLR_LINK_LIBS PkgConfig::LCMS2)
endif()

target_link_libraries(wlroots PUBLIC ${WLR_LINK_LIBS})

# Version script (only export wlr_* / _wlr_*)
target_link_options(wlroots PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/wlroots.syms")

# Install only the library — NO headers to system prefix (issue requirement)
install(TARGETS wlroots
    EXPORT wlrootsTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(EXPORT wlrootsTargets
    FILE wlrootsTargets.cmake
    NAMESPACE PkgConfig::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wlroots
)

# Alias so existing PkgConfig::WLROOTS references resolve transparently
add_library(PkgConfig::WLROOTS ALIAS wlroots)

# ── Export feature/version variables to parent scope (for qwlroots) ──────────
# WLR_HAVE_* must be 1/0 (qwconfig.h.in uses #cmakedefine WLR_HAVE_X @WLR_HAVE_X@)
set(WLR_HAVE_DRM_BACKEND       0 PARENT_SCOPE)
set(WLR_HAVE_X11_BACKEND       0 PARENT_SCOPE)
set(WLR_HAVE_LIBINPUT_BACKEND  0 PARENT_SCOPE)
set(WLR_HAVE_XWAYLAND          0 PARENT_SCOPE)
set(WLR_HAVE_GLES2_RENDERER    0 PARENT_SCOPE)
set(WLR_HAVE_VULKAN_RENDERER   0 PARENT_SCOPE)
set(WLR_HAVE_GBM_ALLOCATOR     0 PARENT_SCOPE)
set(WLR_HAVE_SESSION           0 PARENT_SCOPE)
if(WLR_HAS_DRM_BACKEND)
  set(WLR_HAVE_DRM_BACKEND 1 PARENT_SCOPE)
endif()
if(WLR_HAS_X11_BACKEND)
  set(WLR_HAVE_X11_BACKEND 1 PARENT_SCOPE)
endif()
if(WLR_HAS_LIBINPUT_BACKEND)
  set(WLR_HAVE_LIBINPUT_BACKEND 1 PARENT_SCOPE)
endif()
if(WLR_HAS_XWAYLAND)
  set(WLR_HAVE_XWAYLAND 1 PARENT_SCOPE)
endif()
if(WLR_HAS_GLES2_RENDERER)
  set(WLR_HAVE_GLES2_RENDERER 1 PARENT_SCOPE)
endif()
if(WLR_HAS_VULKAN_RENDERER)
  set(WLR_HAVE_VULKAN_RENDERER 1 PARENT_SCOPE)
endif()
if(WLR_HAS_GBM_ALLOCATOR)
  set(WLR_HAVE_GBM_ALLOCATOR 1 PARENT_SCOPE)
endif()
if(WLR_HAS_SESSION)
  set(WLR_HAVE_SESSION 1 PARENT_SCOPE)
endif()

set(WLROOTS_VERSION       ${WLROOTS_VERSION}       PARENT_SCOPE)
set(WLROOTS_VERSION_MAJOR ${WLROOTS_VERSION_MAJOR} PARENT_SCOPE)
set(WLROOTS_VERSION_MINOR ${WLROOTS_VERSION_MINOR} PARENT_SCOPE)
set(WLROOTS_VERSION_PATCH ${WLROOTS_VERSION_PATCH} PARENT_SCOPE)

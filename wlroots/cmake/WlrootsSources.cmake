# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

# Source list collection for waylib-wlroots.
# Mirrors wlroots 0.19.3 meson.build file lists (static + conditionals).
# Source paths are under WLROOTS_SOURCE_DIR (3rdparty/wlroots).

function(wlroots_collect_sources target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "wlroots_collect_sources: target '${target}' does not exist")
    endif()

    if(NOT WLROOTS_SOURCE_DIR)
        message(FATAL_ERROR "wlroots_collect_sources: WLROOTS_SOURCE_DIR is not set")
    endif()
    set(_wlr_src "${WLROOTS_SOURCE_DIR}")

    # ------------------------------------------------------------------
    # util/ (always)
    # ------------------------------------------------------------------
    set(_util_sources
        util/addon.c
        util/array.c
        util/box.c
        util/env.c
        util/global.c
        util/log.c
        util/matrix.c
        util/rect_union.c
        util/region.c
        util/set.c
        util/shm.c
        util/time.c
        util/token.c
        util/transform.c
        util/utf8.c
    )

    # ------------------------------------------------------------------
    # xcursor/ (always)
    # ------------------------------------------------------------------
    set(_xcursor_sources
        xcursor/wlr_xcursor.c
        xcursor/xcursor.c
    )

    # ------------------------------------------------------------------
    # types/ (always + conditional drm_lease)
    # ------------------------------------------------------------------
    set(_types_sources
        types/data_device/wlr_data_device.c
        types/data_device/wlr_data_offer.c
        types/data_device/wlr_data_source.c
        types/data_device/wlr_drag.c
        types/ext_image_capture_source_v1/base.c
        types/ext_image_capture_source_v1/output.c
        types/output/cursor.c
        types/output/output.c
        types/output/render.c
        types/output/state.c
        types/output/swapchain.c
        types/scene/drag_icon.c
        types/scene/subsurface_tree.c
        types/scene/surface.c
        types/scene/wlr_scene.c
        types/scene/output_layout.c
        types/scene/xdg_shell.c
        types/scene/layer_shell_v1.c
        types/seat/wlr_seat_keyboard.c
        types/seat/wlr_seat_pointer.c
        types/seat/wlr_seat_touch.c
        types/seat/wlr_seat.c
        types/tablet_v2/wlr_tablet_v2_pad.c
        types/tablet_v2/wlr_tablet_v2_tablet.c
        types/tablet_v2/wlr_tablet_v2_tool.c
        types/tablet_v2/wlr_tablet_v2.c
        types/xdg_shell/wlr_xdg_popup.c
        types/xdg_shell/wlr_xdg_positioner.c
        types/xdg_shell/wlr_xdg_shell.c
        types/xdg_shell/wlr_xdg_surface.c
        types/xdg_shell/wlr_xdg_toplevel.c
        types/buffer/buffer.c
        types/buffer/client.c
        types/buffer/dmabuf.c
        types/buffer/readonly_data.c
        types/buffer/resource.c
        types/wlr_alpha_modifier_v1.c
        types/wlr_color_management_v1.c
        types/wlr_compositor.c
        types/wlr_content_type_v1.c
        types/wlr_cursor_shape_v1.c
        types/wlr_cursor.c
        types/wlr_damage_ring.c
        types/wlr_data_control_v1.c
        types/wlr_drm.c
        types/wlr_export_dmabuf_v1.c
        types/wlr_foreign_toplevel_management_v1.c
        types/wlr_ext_image_copy_capture_v1.c
        types/wlr_ext_foreign_toplevel_list_v1.c
        types/wlr_ext_data_control_v1.c
        types/wlr_fractional_scale_v1.c
        types/wlr_gamma_control_v1.c
        types/wlr_idle_inhibit_v1.c
        types/wlr_idle_notify_v1.c
        types/wlr_input_device.c
        types/wlr_input_method_v2.c
        types/wlr_keyboard.c
        types/wlr_keyboard_group.c
        types/wlr_keyboard_shortcuts_inhibit_v1.c
        types/wlr_layer_shell_v1.c
        types/wlr_linux_dmabuf_v1.c
        types/wlr_linux_drm_syncobj_v1.c
        types/wlr_output_layer.c
        types/wlr_output_layout.c
        types/wlr_output_management_v1.c
        types/wlr_output_power_management_v1.c
        types/wlr_output_swapchain_manager.c
        types/wlr_pointer_constraints_v1.c
        types/wlr_pointer_gestures_v1.c
        types/wlr_pointer.c
        types/wlr_presentation_time.c
        types/wlr_primary_selection_v1.c
        types/wlr_primary_selection.c
        types/wlr_region.c
        types/wlr_relative_pointer_v1.c
        types/wlr_screencopy_v1.c
        types/wlr_security_context_v1.c
        types/wlr_server_decoration.c
        types/wlr_session_lock_v1.c
        types/wlr_shm.c
        types/wlr_single_pixel_buffer_v1.c
        types/wlr_subcompositor.c
        types/wlr_switch.c
        types/wlr_tablet_pad.c
        types/wlr_tablet_tool.c
        types/wlr_tearing_control_v1.c
        types/wlr_text_input_v3.c
        types/wlr_touch.c
        types/wlr_transient_seat_v1.c
        types/wlr_viewporter.c
        types/wlr_virtual_keyboard_v1.c
        types/wlr_virtual_pointer_v1.c
        types/wlr_xcursor_manager.c
        types/wlr_xdg_activation_v1.c
        types/wlr_xdg_decoration_v1.c
        types/wlr_xdg_dialog_v1.c
        types/wlr_xdg_foreign_v1.c
        types/wlr_xdg_foreign_v2.c
        types/wlr_xdg_foreign_registry.c
        types/wlr_xdg_output_v1.c
        types/wlr_xdg_system_bell_v1.c
        types/wlr_xdg_toplevel_icon_v1.c
    )
    if(WLR_HAS_DRM_BACKEND)
        list(APPEND _types_sources types/wlr_drm_lease_v1.c)
    endif()

    # ------------------------------------------------------------------
    # backend/ (core always; multi/wayland/headless always; others gated)
    # ------------------------------------------------------------------
    set(_backend_sources
        backend/backend.c
        backend/multi/backend.c
        backend/wayland/backend.c
        backend/wayland/output.c
        backend/wayland/seat.c
        backend/wayland/pointer.c
        backend/wayland/tablet_v2.c
        backend/headless/backend.c
        backend/headless/output.c
    )

    if(WLR_HAS_SESSION)
        list(APPEND _backend_sources backend/session/session.c)
    endif()

    if(WLR_HAS_DRM_BACKEND)
        list(APPEND _backend_sources
            backend/drm/atomic.c
            backend/drm/backend.c
            backend/drm/drm.c
            backend/drm/fb.c
            backend/drm/legacy.c
            backend/drm/monitor.c
            backend/drm/properties.c
            backend/drm/renderer.c
            backend/drm/util.c
        )
        if(HAVE_LIBLIFTOFF)
            list(APPEND _backend_sources backend/drm/libliftoff.c)
        endif()
        # pnpids.c is generated and attached by wlroots_generate_pnpids()
    endif()

    if(WLR_HAS_LIBINPUT_BACKEND)
        list(APPEND _backend_sources
            backend/libinput/backend.c
            backend/libinput/events.c
            backend/libinput/keyboard.c
            backend/libinput/pointer.c
            backend/libinput/switch.c
            backend/libinput/tablet_pad.c
            backend/libinput/tablet_tool.c
            backend/libinput/touch.c
        )
    endif()

    if(WLR_HAS_X11_BACKEND)
        list(APPEND _backend_sources
            backend/x11/backend.c
            backend/x11/input_device.c
            backend/x11/output.c
        )
    endif()

    # ------------------------------------------------------------------
    # render/ (core always; pixman always; gles2/vulkan/allocator/color gated)
    # ------------------------------------------------------------------
    set(_render_sources
        render/color.c
        render/dmabuf.c
        render/drm_format_set.c
        render/drm_syncobj.c
        render/pass.c
        render/pixel_format.c
        render/swapchain.c
        render/wlr_renderer.c
        render/wlr_texture.c
        # pixman always
        render/pixman/pass.c
        render/pixman/pixel_format.c
        render/pixman/renderer.c
        # allocator core always
        render/allocator/allocator.c
        render/allocator/shm.c
        render/allocator/drm_dumb.c
    )

    # dmabuf: linux path when kernel header present, else fallback
    check_include_file("linux/dma-buf.h" _have_linux_dmabuf_h)
    if(_have_linux_dmabuf_h AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND _render_sources render/dmabuf_linux.c)
    else()
        list(APPEND _render_sources render/dmabuf_fallback.c)
    endif()

    if(WLR_HAS_GLES2_RENDERER)
        list(APPEND _render_sources
            render/egl.c
            render/gles2/pass.c
            render/gles2/pixel_format.c
            render/gles2/renderer.c
            render/gles2/texture.c
        )
        # GLES2 shader headers attached by wlroots_generate_shaders()
    endif()

    if(WLR_HAS_VULKAN_RENDERER)
        list(APPEND _render_sources
            render/vulkan/pass.c
            render/vulkan/renderer.c
            render/vulkan/texture.c
            render/vulkan/vulkan.c
            render/vulkan/util.c
            render/vulkan/pixel_format.c
        )
        # Vulkan shader headers attached by wlroots_generate_shaders()
    endif()

    if(WLR_HAS_GBM_ALLOCATOR)
        list(APPEND _render_sources render/allocator/gbm.c)
    endif()

    if(WLR_HAS_UDMABUF_ALLOCATOR)
        list(APPEND _render_sources render/allocator/udmabuf.c)
    endif()

    if(WLR_HAS_COLOR_MANAGEMENT)
        list(APPEND _render_sources render/color_lcms2.c)
    else()
        list(APPEND _render_sources render/color_fallback.c)
    endif()

    # ------------------------------------------------------------------
    # xwayland/ (conditional)
    # ------------------------------------------------------------------
    set(_xwayland_sources)
    if(WLR_HAS_XWAYLAND)
        set(_xwayland_sources
            xwayland/selection/dnd.c
            xwayland/selection/incoming.c
            xwayland/selection/outgoing.c
            xwayland/selection/selection.c
            xwayland/server.c
            xwayland/shell.c
            xwayland/sockets.c
            xwayland/xwayland.c
            xwayland/xwm.c
        )
    endif()

    # ------------------------------------------------------------------
    # Attach all sources
    # ------------------------------------------------------------------
    set(_all_sources
        ${_util_sources}
        ${_xcursor_sources}
        ${_types_sources}
        ${_backend_sources}
        ${_render_sources}
        ${_xwayland_sources}
    )

    list(TRANSFORM _all_sources PREPEND "${_wlr_src}/")

    target_sources(${target} PRIVATE ${_all_sources})
endfunction()

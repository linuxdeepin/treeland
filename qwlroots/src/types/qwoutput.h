// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_output.h>
#include <wlr/backend/drm.h>
#undef static
#include <wlr/backend/wayland.h>
#ifdef WLR_HAVE_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/backend/headless.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(output)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(frame)
    QW_SIGNAL(damage, wlr_output_event_damage*)
    QW_SIGNAL(needs_frame)
    QW_SIGNAL(precommit, wlr_output_event_precommit*)
    QW_SIGNAL(commit, wlr_output_event_commit*)
    QW_SIGNAL(present, wlr_output_event_present*)
    QW_SIGNAL(bind, wlr_output_event_bind*)
    QW_SIGNAL(description)
    QW_SIGNAL(request_state, wlr_output_event_request_state*)

public:
    QW_FUNC_MEMBER(output, is_drm, bool)
    QW_FUNC_MEMBER(output, is_wl, bool)
#ifdef WLR_HAVE_X11_BACKEND
    QW_FUNC_MEMBER(output, is_x11, bool)
#endif
    QW_FUNC_MEMBER(output, is_headless, bool)

#if WLR_VERSION_MAJOR == 0 && WLR_VERSION_MINOR < 18
    QW_FUNC_MEMBER(output, enable, void, bool enable)
#endif
    QW_FUNC_MEMBER(output, create_global, void)
    QW_FUNC_MEMBER(output, destroy_global, void)
    QW_FUNC_MEMBER(output, init_render, bool, wlr_allocator *allocator, wlr_renderer *renderer)
    QW_FUNC_MEMBER(output, preferred_mode, wlr_output_mode *)

#if WLR_VERSION_MINOR < 18
    QW_FUNC_MEMBER(output, set_mode, void, wlr_output_mode *mode)
    QW_FUNC_MEMBER(output, set_custom_mode, void, int32_t width, int32_t height, int32_t refresh)
    QW_FUNC_MEMBER(output, set_transform, void, enum wl_output_transform transform)
    QW_FUNC_MEMBER(output, enable_adaptive_sync, void, bool enabled)
    QW_FUNC_MEMBER(output, set_render_format, void, uint32_t format)
    QW_FUNC_MEMBER(output, set_scale, void, float scale)
    QW_FUNC_MEMBER(output, set_subpixel, void, enum wl_output_subpixel subpixel)
    QW_FUNC_MEMBER(output, set_damage, void, const pixman_region32_t *damage)
#endif
    QW_FUNC_MEMBER(output, set_name, void, const char *name)
    QW_FUNC_MEMBER(output, set_description, void, const char *desc)
    QW_FUNC_MEMBER(output, schedule_done, void)
    QW_FUNC_MEMBER(output, transformed_resolution, void, int *width, int *height)
    QW_FUNC_MEMBER(output, effective_resolution, void, int *width, int *height)

#if WLR_VERSION_MINOR < 18
    QW_FUNC_MEMBER(output, attach_render, bool, int *buffer_age)
    QW_FUNC_MEMBER(output, attach_buffer, void, wlr_buffer *buffer)
    QW_FUNC_MEMBER(output, preferred_read_format, uint32_t)
#endif

    QW_FUNC_MEMBER(output, lock_attach_render, void, bool lock)

#if WLR_VERSION_MINOR < 18
    QW_FUNC_MEMBER(output, test, bool)
    QW_FUNC_MEMBER(output, commit, bool)
    QW_FUNC_MEMBER(output, rollback, void)
    QW_FUNC_MEMBER(output, set_gamma, void, size_t size, const uint16_t *r, const uint16_t *g, const uint16_t *b)
#endif

    QW_FUNC_MEMBER(output, test_state, bool, const wlr_output_state *state)
    QW_FUNC_MEMBER(output, commit_state, bool, const wlr_output_state *state)

    QW_FUNC_MEMBER(output, schedule_frame, void)
    QW_FUNC_MEMBER(output, get_gamma_size, size_t)
    QW_FUNC_MEMBER(output, lock_software_cursors, void, bool lock)

#if WLR_VERSION_MAJOR == 0 && WLR_VERSION_MINOR < 18
    QW_FUNC_MEMBER(output, render_software_cursors, void, const pixman_region32_t *damage)
#endif

    QW_FUNC_MEMBER(output, get_primary_formats, const wlr_drm_format_set *, uint32_t buffer_caps)
    QW_FUNC_MEMBER(output, add_software_cursors_to_render_pass, void, wlr_render_pass *render_pass, const pixman_region32_t *damage)
    QW_FUNC_MEMBER(output, configure_primary_swapchain, bool, const wlr_output_state *state, wlr_swapchain **swapchain)

protected:
    QW_FUNC_MEMBER(output, destroy, void)
};

class QW_CLASS_REINTERPRET_CAST(output_cursor)
{
public:
    QW_FUNC_STATIC(output_cursor, create, qw_output_cursor *, wlr_output *output)

    QW_FUNC_MEMBER(output_cursor, set_buffer, bool, wlr_buffer *buffer, int32_t hotspot_x, int32_t hotspot_y)
    QW_FUNC_MEMBER(output_cursor, move, bool, double x, double y)

private:
    friend class qw_reinterpret_cast;
    QW_FUNC_MEMBER(output_cursor, destroy, void)
};

class QW_CLASS_BOX(output_state)
{
public:
    QW_FUNC_MEMBER(output_state, set_enabled, void, bool enabled)
    QW_FUNC_MEMBER(output_state, set_mode, void, wlr_output_mode *mode)
    QW_FUNC_MEMBER(output_state, set_custom_mode, void, int32_t width, int32_t height, int32_t refresh)
    QW_FUNC_MEMBER(output_state, set_scale, void, float scale)
    QW_FUNC_MEMBER(output_state, set_transform, void, enum wl_output_transform transform)
    QW_FUNC_MEMBER(output_state, set_adaptive_sync_enabled, void, bool enabled)
    QW_FUNC_MEMBER(output_state, set_render_format, void, uint32_t format)
    QW_FUNC_MEMBER(output_state, set_subpixel, void, enum wl_output_subpixel subpixel)
    QW_FUNC_MEMBER(output_state, set_buffer, void, wlr_buffer *buffer)
    QW_FUNC_MEMBER(output_state, set_gamma_lut, bool, size_t ramp_size, const uint16_t *r, const uint16_t *g, const uint16_t *b)
    QW_FUNC_MEMBER(output_state, set_damage, void, const pixman_region32_t *damage)
    QW_FUNC_MEMBER(output_state, set_layers, void, wlr_output_layer_state *layers, size_t layers_len)
    QW_FUNC_MEMBER(output_state, copy, bool, const wlr_output_state *src)

private:
    friend class qw_class_box;
    QW_FUNC_MEMBER(output_state, init, void)
    QW_FUNC_MEMBER(output_state, finish, void)
};

QW_END_NAMESPACE

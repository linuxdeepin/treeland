// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <stdint.h>

struct toplevel_capture_state {
    int valid;
    int titlebar_height;
    int width, height;
    int bounds_x, bounds_y, bounds_width, bounds_height;
    int geometry_unchanged;
    int screen_unchanged;
    int render_state_unchanged;
    int capture_frames_checked;
    int capture_paints;
    int offscreen_viewports;
    int capture_sources;
    int isolated_inputs;
    int shared_delegates;
    int capture_only_did_not_commit;
    int occluder_visible;
    int maximized;
    int dragging;
    int border_visible;
    uint32_t border_color;
    int position_x, position_y;
};

struct toplevel_capture_move {
    int x, y;
    struct toplevel_capture_state state;
};

struct toplevel_capture_output_state {
    int output_count;
    int enabled_outputs;
    int target_on_other_output;
    int capture_output_enabled;
    int viewport_unchanged;
    int offscreen_viewports;
    int enabled_capture_viewports;
    int disabled_frames;
    int disabled_frames_did_not_commit;
    int render_attempts;
    int render_enabled;
};

#ifdef __cplusplus
extern "C" {
#endif
void toplevel_capture_prepare(void *data);
void toplevel_capture_check_screen(void *data);
void toplevel_capture_flush_source_deletions(void *data);
void toplevel_capture_repaint_screen(void *data);
void toplevel_capture_render_offscreen(void *data);
void toplevel_capture_hold_buffers(void *data);
void toplevel_capture_release_buffers(void *data);
void toplevel_capture_move_partly_offscreen(void *data);
void toplevel_capture_save_screen(void *data);
void toplevel_capture_pause_checks(void *data);
void toplevel_capture_move_window(void *data);
void toplevel_capture_wait_transition(void *data);
void toplevel_capture_drag_begin(void *data);
void toplevel_capture_drag_move(void *data);
void toplevel_capture_drag_end(void *data);
void toplevel_capture_add_output(void *data);
void toplevel_capture_read_output_state(void *data);
void toplevel_capture_sync_output(void *data);
void toplevel_capture_set_border(void *data);
void toplevel_capture_toggle_title_color(void *data);
#ifdef __cplusplus
}
#endif

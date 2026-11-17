// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

struct toplevel_capture_state {
    int valid;
    int titlebar_height;
    int width, height;
    int bounds_x, bounds_y, bounds_width, bounds_height;
    int geometry_unchanged;
    int screen_unchanged;
    int render_state_unchanged;
    int capture_frames_checked;
    int offscreen_viewports;
    int isolated_inputs;
    int capture_only_did_not_commit;
    int occluder_visible;
    int maximized;
    int dragging;
    int border_visible;
    int position_x, position_y;
};

struct toplevel_capture_move {
    int x, y;
    struct toplevel_capture_state state;
};

#ifdef __cplusplus
extern "C" {
#endif
void toplevel_capture_prepare(void *data);
void toplevel_capture_check_screen(void *data);
void toplevel_capture_repaint_screen(void *data);
void toplevel_capture_render_offscreen(void *data);
void toplevel_capture_move_partly_offscreen(void *data);
void toplevel_capture_save_screen(void *data);
void toplevel_capture_pause_checks(void *data);
void toplevel_capture_move_window(void *data);
void toplevel_capture_wait_transition(void *data);
void toplevel_capture_drag_begin(void *data);
void toplevel_capture_drag_move(void *data);
void toplevel_capture_drag_end(void *data);
#ifdef __cplusplus
}
#endif

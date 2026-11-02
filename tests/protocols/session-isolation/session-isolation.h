// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

struct isolation_state {
    char socket_name[512];
    int ready;
    double a_scale;
    double b_scale;
    double hardware_scale;
    int a_logical_width;
    int b_logical_width;
    int independent_outputs;
    int a_bottom_zone;
    int b_bottom_zone;
    int active_bottom_zone;
    int a_panel_at_bottom;
    int b_panel_at_bottom;
    int a_visible_windows;
    int b_visible_windows;
    int a_windows;
    int b_windows;
    int independent_workspaces;
    int a_current_workspace;
    int b_current_workspace;
    int preview_surfaces;
    int preview_requests;
    int foreign_preview_surfaces;
    int independent_seats;
    int protocol_socket_ownership;
    int independent_desktops;
    int inactive_surfaces_stopped;
    int independent_managers;
    int independent_protocol_domains;
    int active_managers;
    int a_shortcut;
    int b_shortcut;
    int foreign_shortcut;
};

#ifdef __cplusplus
extern "C" {
#endif
void isolation_read(void *data);
void isolation_prepare_background(void *data);
void isolation_switch_b(void *data);
void isolation_scale_a(void *data);
void isolation_restore_a(void *data);
void isolation_reactivate_b(void *data);
#ifdef __cplusplus
}
#endif

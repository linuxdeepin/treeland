// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef FOREIGN_TOPLEVEL_MANAGER_TEST_H
#define FOREIGN_TOPLEVEL_MANAGER_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);

#include "client-connection.h"
#include "xdg-toplevel-client.h"

#define TEST_MSG_MAX 256

struct test_result {
    const char *name;
    int         failed;
    char        message[TEST_MSG_MAX];
};

struct ftm_server_state {
    int      output_ready;
    int      wrapper_created;
    int      wrapper_in_workspace;
    int      mapped_xdg_toplevel;
    int      wrapper_minimized;
    int      wrapper_maximized;
    int      wrapper_fullscreen;
    int      wrapper_activated;
    int      wrapper_focused;
    int      wrapper_skip_dock_preview;
    int      wrapper_x;
    int      wrapper_y;
    int      icon_x;
    int      icon_y;
    int      icon_width;
    int      icon_height;
    int      preview_fired;
    int      preview_x;
    int      preview_y;
    uint32_t preview_direction;
    int      preview_surface_count;
    int      tooltip_fired;
    char     tooltip[64];
    int      tooltip_x;
    int      tooltip_y;
    uint32_t tooltip_direction;
    int      close_fired;
};

struct test_ctx {
    struct client_connection connection;
    struct wl_display    *display;

    struct treeland_foreign_toplevel_manager_v1  *manager;
    struct treeland_dock_preview_context_v1      *context;
    struct treeland_foreign_toplevel_handle_v1   *handle;
    struct wl_seat                                *seat;
    struct xdg_toplevel_client             xdg_toplevel;

    int context_enter_received;
    int context_leave_received;
    int manager_finished_received;
    int handle_count;
    int handle_closed_count;
    uint32_t handle_identifier;

    struct test_result *results;
    int                 result_count;
    int                 result_cap;
};

void test_init(struct test_ctx *ctx);
void test_destroy(struct test_ctx *ctx);
int  test_add(struct test_ctx *ctx, const char *name);
void test_fail(struct test_ctx *ctx, int idx, const char *fmt, ...);
void test_pass(struct test_ctx *ctx, int idx);

int test_print_results(struct test_ctx *ctx);
void test_cleanup(struct test_ctx *ctx);

#ifdef __cplusplus
}
#endif
#endif

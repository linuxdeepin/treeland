// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef DDE_SHELL_TEST_H
#define DDE_SHELL_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);

#include "client-connection.h"

#define TEST_MSG_MAX 256

struct dde_shell_surface_state {
    int position_x;
    int position_y;
    int role_overlay;
    int auto_placement;
    int skip_switcher;
    int skip_dock_preview;
    int skip_multitask_view;
    int accept_keyboard_focus;
};

struct test_result {
    const char *name;
    int         failed;
    char        message[TEST_MSG_MAX];
};

struct test_ctx {
    struct client_connection connection;
    struct wl_display    *display;

    struct wl_compositor *compositor;
    struct wl_seat       *seat;
    struct wl_output     *output;

    struct treeland_dde_shell_manager_v1  *manager;
    struct treeland_window_overlap_checker *checker;
    struct treeland_dde_shell_surface_v1  *shell_surface;
    struct treeland_dde_active_v1         *active;
    struct treeland_multitaskview_v1      *multitaskview;
    struct treeland_window_picker_v1      *picker;
    struct treeland_lockscreen_v1         *lockscreen;
    struct wl_surface                     *test_surface;

    int checker_enter_received;
    int checker_leave_received;
    int active_in_received;
    int active_out_received;
    int start_drag_received;
    int drop_received;
    int picker_window_received;
    int picker_pid;

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

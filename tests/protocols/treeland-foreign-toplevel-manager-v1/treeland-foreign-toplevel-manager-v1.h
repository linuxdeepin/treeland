// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef FOREIGN_TOPLEVEL_MANAGER_TEST_H
#define FOREIGN_TOPLEVEL_MANAGER_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);

#include "protocol-test-client.h"

#define TEST_MSG_MAX 256

struct test_result {
    const char *name;
    int         failed;
    char        message[TEST_MSG_MAX];
};


struct ftm_server_state {
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
    struct protocol_test_connection connection;
    struct wl_display    *display;


    struct wl_compositor *compositor;
    struct wl_xdg_wm_base *xdg_wm_base;


    struct treeland_foreign_toplevel_manager_v1  *manager;
    struct treeland_dock_preview_context_v1      *context;
    struct wl_surface                            *test_surface;
    struct xdg_surface                           *xdg_surface;
    struct xdg_toplevel                          *xdg_toplevel;


    int context_enter_received;
    int context_leave_received;
    int manager_finished_received;


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

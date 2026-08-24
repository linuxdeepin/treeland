// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef WALLPAPER_SHELL_TEST_H
#define WALLPAPER_SHELL_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);

#include "client-connection.h"

#define TEST_MSG_MAX 256

struct test_result {
    const char *name;
    int         failed;
    char        message[TEST_MSG_MAX];
};

struct test_ctx {
    struct client_connection connection;
    struct wl_display    *display;

    struct wl_compositor *compositor;
    struct treeland_wallpaper_notifier_v1 *notifier;

    struct treeland_wallpaper_shell_v1   *shell;
    struct treeland_wallpaper_surface_v1 *wallpaper_surface;
    struct wl_surface                    *test_surface;

    int      play_received;
    int      pause_received;
    int      slow_down_received;
    uint32_t slow_down_duration;
    int      notifier_add_received;
    int      notifier_remove_received;
    uint32_t notifier_add_type;
    char     notifier_add_source[TEST_MSG_MAX];
    char     notifier_remove_source[TEST_MSG_MAX];

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

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef WALLPAPER_MANAGER_TEST_H
#define WALLPAPER_MANAGER_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);

#include "client-connection.h"

#define TEST_MSG_MAX 256
#define WM_TEST_SOURCE "/usr/share/wallpapers/deepin-wallpapers/test-wallpaper.jpg"

struct wm_server_state {
    int wallpaper_created;
    int second_created;
    int output_valid;
    int has_username;
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
    struct wl_output     *output;

    struct treeland_wallpaper_manager_v1 *manager;
    struct treeland_wallpaper_v1         *wallpaper;
    struct treeland_wallpaper_v1         *wallpaper2;
    struct wl_surface                    *test_surface;

    int  failed_received;
    int  failed_error;
    char failed_source[TEST_MSG_MAX];
    int  changed_received;
    int  changed_role;
    int  changed_type;
    char changed_source[TEST_MSG_MAX];

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

/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Pure-C Wayland client for the treeland-wallpaper-color-v1 protocol test.
 */
#ifndef WALLPAPER_COLOR_TEST_H
#define WALLPAPER_COLOR_TEST_H

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

struct test_ctx {
    struct protocol_test_connection connection;
    struct wl_display    *display;

    /* protocol objects */
    struct treeland_wallpaper_color_manager_v1 *manager;

    /* event verification */
    int  output_color_count;
    int  output_color_isdark;
    char output_color_name[128];

    /* results */
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

/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Pure-C Wayland client for the treeland-wallpaper-manager-unstable-v1 protocol test.
 */
#ifndef WALLPAPER_MANAGER_TEST_H
#define WALLPAPER_MANAGER_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);

#include "protocol-test-client.h"

#define TEST_MSG_MAX 256
#define WM_TEST_SOURCE "/usr/share/wallpapers/deepin-wallpapers/test-wallpaper.jpg"

/* Server-side state reported back through protocol_test_invoke_server(). */
struct wm_server_state {
    int wallpaper_created; /* first treeland_wallpaper_v1 object exists */
    int second_created;    /* the null-surface wallpaper object exists */
    int output_valid;      /* object's WOutput is alive */
    int has_username;      /* object carries the client user's name */
};

struct test_result {
    const char *name;
    int         failed;
    char        message[TEST_MSG_MAX];
};

struct test_ctx {
    struct protocol_test_connection connection;
    struct wl_display    *display;

    /* bound globals */
    struct wl_compositor *compositor;
    struct wl_output     *output;

    /* protocol objects */
    struct treeland_wallpaper_manager_v1 *manager;
    struct treeland_wallpaper_v1         *wallpaper;
    struct treeland_wallpaper_v1         *wallpaper2;
    struct wl_surface                    *test_surface;

    /* event verification */
    int  failed_received;
    int  failed_error;
    char failed_source[TEST_MSG_MAX];
    int  changed_received;
    int  changed_role;
    int  changed_type;
    char changed_source[TEST_MSG_MAX];

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

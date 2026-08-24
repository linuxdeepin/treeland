// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef PRELAUNCH_SPLASH_TEST_H
#define PRELAUNCH_SPLASH_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);
void splash_query_state(void *data);

#include "client-connection.h"

#define TEST_MSG_MAX 256
#define SPLASH_TEST_MAX_REQUESTS 8

struct test_result {
    const char *name;
    int         failed;
    char        message[TEST_MSG_MAX];
};

struct splash_server_state {
    int request_count;
    int close_count;
    struct {
        char app_id[64];
        char instance_id[64];
        int  icon_non_null;
    } requests[SPLASH_TEST_MAX_REQUESTS];
    char last_close_app_id[64];
    char last_close_instance_id[64];
};

struct test_ctx {
    struct client_connection connection;
    struct wl_display *display;

    struct treeland_prelaunch_splash_manager_v2 *manager;
    struct wl_shm *shm;

    struct treeland_prelaunch_splash_v2 *splash1;
    struct treeland_prelaunch_splash_v2 *splash2;
    struct treeland_prelaunch_splash_v2 *splash3;
    struct wl_buffer *icon_buffer;

    struct splash_server_state server;

    struct test_result *results;
    int                 result_count;
    int                 result_cap;
};

void test_init(struct test_ctx *ctx);
void test_destroy(struct test_ctx *ctx);
int  test_add(struct test_ctx *ctx, const char *name);
void test_fail(struct test_ctx *ctx, int idx, const char *fmt, ...);
void test_pass(struct test_ctx *ctx, int idx);
int  test_print_results(struct test_ctx *ctx);
void test_cleanup(struct test_ctx *ctx);

#ifdef __cplusplus
}
#endif
#endif

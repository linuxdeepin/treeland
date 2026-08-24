// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef TREELAND_SHORTCUT_MANAGER_V2_TEST_H
#define TREELAND_SHORTCUT_MANAGER_V2_TEST_H

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

    struct treeland_shortcut_manager_v2 *manager;
    struct treeland_shortcut_capture_v1 *capture;
    struct wl_surface                   *test_surface;

    int      commit_success_received;
    int      commit_failure_received;
    char     commit_failure_name[64];
    uint32_t commit_failure_error;
    int      capture_captured_received;
    char     capture_captured_key[64];
    int      capture_failed_received;
    uint32_t capture_failed_reason;

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

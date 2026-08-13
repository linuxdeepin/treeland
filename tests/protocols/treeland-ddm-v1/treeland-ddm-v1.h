// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
/*
 *
 * Pure-C Wayland client for the treeland-ddm-v1 protocol test.
 */
#ifndef DDM_V1_TEST_H
#define DDM_V1_TEST_H

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

    /* socket name, kept so the test can open additional clients */
    const char *socket_name;

    /* second client: keeps the global bound while the first client is
     * disconnected, and later drives the server to observe the resulting
     * module state */
    struct protocol_test_connection aux;

    /* fresh observer connection used after every client is gone */
    struct protocol_test_connection checker;

    /* bound global */
    struct treeland_ddm_v1 *ddm;
    int bound_version;

    /* event verification (the module never emits these, so the test asserts
     * that no unsolicited events arrive) */
    int switch_to_vt_received;
    int acquire_vt_received;

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

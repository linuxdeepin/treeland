/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Pure-C Wayland client for the treeland-capture-unstable-v1 protocol test.
 */
#ifndef TREELAND_CAPTURE_TEST_H
#define TREELAND_CAPTURE_TEST_H

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
    const char           *socket_name;

    /* bound globals */
    struct treeland_capture_manager_v1 *manager;

    /* protocol objects */
    struct treeland_capture_context_v1 *context_a;
    struct treeland_capture_context_v1 *context_b;
    struct treeland_capture_context_v1 *context_c;

    /* event verification */
    int      a_source_failed_received;
    int      b_source_failed_received;
    uint32_t b_source_failed_reason;
    int      c_source_failed_received;

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

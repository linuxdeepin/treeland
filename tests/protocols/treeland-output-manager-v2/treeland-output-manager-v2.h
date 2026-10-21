// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef TREELAND_OUTPUT_MANAGER_V2_TEST_H
#define TREELAND_OUTPUT_MANAGER_V2_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);

// Runs on the compositor thread via invoke_on_server_thread().
void protocol_test_disable_output(void);

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

    struct wl_output *output;

    struct treeland_output_manager_v2          *manager;
    struct treeland_output_picture_control_v2  *picture_control;
    struct treeland_output_picture_control_v2  *unknown_picture_control;

    /* v2 addresses outputs by the uuid served by treeland-output-uuid-v1. */
    struct treeland_output_uuid_manager_v1 *uuid_manager;
    struct treeland_output_uuid_v1         *uuid_object;
    char                                     uuid[128];
    int                                      uuid_received;

    const char *socket_name;

    int         primary_output_received;
    int         primary_output_count;
    char        primary_output_uuid[128];

    int         primary_output_failed_received;
    uint32_t    primary_output_failed_error;

    int         result_received;
    uint32_t    result_value;
    /* Number of result events seen so far. Needed to tell the result that
     * invalid_output object creation emits by itself apart from the one a
     * later commit produces. Not to be confused with result_count below,
     * which counts the entries of the results array. */
    int         result_event_count;

    int         color_temperature_received;
    uint32_t    color_temperature_value;

    int         brightness_received;
    wl_fixed_t  brightness_value;

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

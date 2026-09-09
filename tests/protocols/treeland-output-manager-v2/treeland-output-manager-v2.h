// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef TREELAND_OUTPUT_MANAGER_V2_TEST_H
#define TREELAND_OUTPUT_MANAGER_V2_TEST_H

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

    struct wl_output *output;

    struct treeland_output_manager_v2          *manager;
    struct treeland_output_picture_control_v2  *picture_control;

    const char *socket_name;

    int         primary_output_received;
    int         primary_output_count;
    struct wl_output *primary_output_obj;

    int         primary_output_failed_received;
    uint32_t    primary_output_failed_error;

    int         result_received;
    uint32_t    result_value;

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

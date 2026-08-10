/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Pure-C Wayland client for the treeland-virtual-output-manager-v1 protocol test.
 */
#ifndef VIRTUAL_OUTPUT_MANAGER_TEST_H
#define VIRTUAL_OUTPUT_MANAGER_TEST_H

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

/* Event state recorded per treeland_virtual_output_v1 object. */
struct virtual_output_state {
    int      outputs_count;
    char     outputs_name[64];
    char     outputs_list[256];   /* array names joined with spaces */
    int      error_count;
    uint32_t error_code;
    char     error_message[128];
};

struct test_ctx {
    struct protocol_test_connection connection;
    struct wl_display *display;

    /* bound globals */
    struct treeland_virtual_output_manager_v1 *manager;

    /* protocol objects */
    struct treeland_virtual_output_v1 *virtual_output;   /* created with valid args */
    struct treeland_virtual_output_v1 *err_empty;        /* created with empty name */
    struct treeland_virtual_output_v1 *err_single;       /* created with one output */
    struct treeland_virtual_output_v1 *err_dup;          /* created with duplicate name */
    struct treeland_virtual_output_v1 *fetched;          /* fetched via get_virtual_output */

    /* per-object event state */
    struct virtual_output_state created;
    struct virtual_output_state empty;
    struct virtual_output_state single;
    struct virtual_output_state dup;
    struct virtual_output_state fetched_state;

    /* manager events */
    int  list_event_count;
    char list_event_names[256];

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
int  test_print_results(struct test_ctx *ctx);
void test_cleanup(struct test_ctx *ctx);

#ifdef __cplusplus
}
#endif
#endif

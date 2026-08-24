// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-virtual-output-manager-v1.h"
#include "server-bridge-api.h"
#include "treeland-virtual-output-manager-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void virtual_output_emit_outputs(void *data);
extern void virtual_output_emit_error(void *data);

struct test_case {
    const char *name;
    int (*run)(struct test_ctx *ctx);
};

void test_init(struct test_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->result_cap = 32;
    ctx->results = calloc(ctx->result_cap, sizeof(*ctx->results));
}

void test_destroy(struct test_ctx *ctx)
{
    free(ctx->results);
    memset(ctx, 0, sizeof(*ctx));
}

int test_add(struct test_ctx *ctx, const char *name)
{
    if (ctx->result_count == ctx->result_cap) {
        ctx->result_cap *= 2;
        ctx->results = realloc(ctx->results, (size_t)ctx->result_cap * sizeof(*ctx->results));
    }
    const int index = ctx->result_count++;
    ctx->results[index] = (struct test_result) { .name = name };
    return index;
}

void test_fail(struct test_ctx *ctx, int index, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(ctx->results[index].message, TEST_MSG_MAX, format, arguments);
    va_end(arguments);
    ctx->results[index].failed = 1;
}

void test_pass(struct test_ctx *ctx, int index)
{
    ctx->results[index].failed = 0;
}

int test_print_results(struct test_ctx *ctx)
{
    int failed = 0;
    printf("\n=== results ===\n");
    for (int i = 0; i < ctx->result_count; ++i) {
        printf("  [%s] %s", ctx->results[i].failed ? "FAIL" : "PASS", ctx->results[i].name);
        if (ctx->results[i].failed) {
            printf(" -- %s", ctx->results[i].message);
            ++failed;
        }
        printf("\n");
    }
    printf("%d/%d passed\n", ctx->result_count - failed, ctx->result_count);
    return failed == 0;
}

static struct virtual_output_state *state_for_object(struct test_ctx *ctx,
                                                     struct treeland_virtual_output_v1 *obj)
{
    if (obj == ctx->virtual_output)
        return &ctx->created;
    if (obj == ctx->fetched)
        return &ctx->fetched_state;
    if (obj == ctx->err_empty)
        return &ctx->empty;
    if (obj == ctx->err_single)
        return &ctx->single;
    if (obj == ctx->err_dup)
        return &ctx->dup;
    return NULL;
}

static void outputs_event(void *data, struct treeland_virtual_output_v1 *obj,
                          const char *name, struct wl_array *outputs)
{
    struct test_ctx *ctx = data;
    struct virtual_output_state *state = state_for_object(ctx, obj);
    if (!state)
        return;
    state->outputs_count++;
    snprintf(state->outputs_name, sizeof(state->outputs_name), "%s", name ? name : "");

    size_t used = 0;
    const char *cursor = outputs->data;
    const char *end = cursor + outputs->size;
    state->outputs_list[0] = '\0';
    while (cursor < end && *cursor) {
        const size_t len = strlen(cursor);
        if (used + len + 2 > sizeof(state->outputs_list))
            break;
        if (used)
            state->outputs_list[used++] = ' ';
        memcpy(state->outputs_list + used, cursor, len);
        used += len;
        state->outputs_list[used] = '\0';
        cursor += len + 1;
    }
}

static void error_event(void *data, struct treeland_virtual_output_v1 *obj,
                        uint32_t code, const char *message)
{
    struct test_ctx *ctx = data;
    struct virtual_output_state *state = state_for_object(ctx, obj);
    if (!state)
        return;
    state->error_count++;
    state->error_code = code;
    snprintf(state->error_message, sizeof(state->error_message), "%s", message ? message : "");
}

static const struct treeland_virtual_output_v1_listener virtual_output_listener = {
    .outputs = outputs_event,
    .error = error_event,
};

static void manager_list_event(void *data, struct treeland_virtual_output_manager_v1 *manager,
                               struct wl_array *names)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->list_event_count++;
    size_t used = 0;
    const char *cursor = names->data;
    const char *end = cursor + names->size;
    ctx->list_event_names[0] = '\0';
    while (cursor < end && *cursor) {
        const size_t len = strlen(cursor);
        if (used + len + 2 > sizeof(ctx->list_event_names))
            break;
        if (used)
            ctx->list_event_names[used++] = ' ';
        memcpy(ctx->list_event_names + used, cursor, len);
        used += len;
        ctx->list_event_names[used] = '\0';
        cursor += len + 1;
    }
}

static const struct treeland_virtual_output_manager_v1_listener manager_listener = {
    .virtual_output_list = manager_list_event,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->manager = client_bind(&ctx->connection, "treeland_virtual_output_manager_v1",
                                      &treeland_virtual_output_manager_v1_interface, 1);
    if (ctx->manager)
        treeland_virtual_output_manager_v1_add_listener(ctx->manager, &manager_listener, ctx);
    return ctx->manager != NULL;
}

static int fill_string_array(struct wl_array *array, const char *const *strings, size_t count)
{
    wl_array_init(array);
    for (size_t i = 0; i < count; ++i) {
        const size_t len = strlen(strings[i]);
        char *slot = wl_array_add(array, len + 1);
        if (!slot) {
            wl_array_release(array);
            return 0;
        }
        memcpy(slot, strings[i], len + 1);
    }
    return 1;
}

static int create_valid(struct test_ctx *ctx)
{
    static const char *const outputs[] = { "DP-1", "HDMI-1" };
    struct wl_array array;
    if (!fill_string_array(&array, outputs, 2))
        return 0;
    ctx->virtual_output =
        treeland_virtual_output_manager_v1_create_virtual_output(ctx->manager, "group1", &array);
    wl_array_release(&array);
    if (ctx->virtual_output)
        treeland_virtual_output_v1_add_listener(ctx->virtual_output, &virtual_output_listener, ctx);
    return ctx->virtual_output != NULL;
}

static int create_empty_name(struct test_ctx *ctx)
{
    static const char *const outputs[] = { "DP-1", "HDMI-1" };
    struct wl_array array;
    if (!fill_string_array(&array, outputs, 2))
        return 0;
    ctx->err_empty =
        treeland_virtual_output_manager_v1_create_virtual_output(ctx->manager, "", &array);
    wl_array_release(&array);
    if (ctx->err_empty)
        treeland_virtual_output_v1_add_listener(ctx->err_empty, &virtual_output_listener, ctx);
    return ctx->err_empty != NULL;
}

static int create_empty_outputs(struct test_ctx *ctx)
{

    struct wl_array array;
    wl_array_init(&array);
    ctx->err_single =
        treeland_virtual_output_manager_v1_create_virtual_output(ctx->manager, "group2", &array);
    wl_array_release(&array);
    if (ctx->err_single)
        treeland_virtual_output_v1_add_listener(ctx->err_single, &virtual_output_listener, ctx);
    return ctx->err_single != NULL;
}

static int create_duplicate_name(struct test_ctx *ctx)
{
    static const char *const outputs[] = { "DP-1", "VGA-1" };
    struct wl_array array;
    if (!fill_string_array(&array, outputs, 2))
        return 0;
    ctx->err_dup =
        treeland_virtual_output_manager_v1_create_virtual_output(ctx->manager, "group1", &array);
    wl_array_release(&array);
    if (ctx->err_dup)
        treeland_virtual_output_v1_add_listener(ctx->err_dup, &virtual_output_listener, ctx);
    return ctx->err_dup != NULL;
}

static int get_list(struct test_ctx *ctx)
{
    treeland_virtual_output_manager_v1_get_virtual_output_list(ctx->manager);
    return 1;
}

static int get_existing(struct test_ctx *ctx)
{
    ctx->fetched = treeland_virtual_output_manager_v1_get_virtual_output(ctx->manager, "group1");
    if (ctx->fetched)
        treeland_virtual_output_v1_add_listener(ctx->fetched, &virtual_output_listener, ctx);
    return ctx->fetched != NULL;
}

static int emit_outputs(struct test_ctx *ctx)
{
    (void)ctx;
    return invoke_on_server_thread(virtual_output_emit_outputs, NULL);
}

static int emit_error(struct test_ctx *ctx)
{
    (void)ctx;
    return invoke_on_server_thread(virtual_output_emit_error, NULL);
}

static int outputs_created_received(struct test_ctx *ctx)
{
    return ctx->created.outputs_count == 1 &&
           strcmp(ctx->created.outputs_name, "group1") == 0 &&
           strcmp(ctx->created.outputs_list, "DP-1 HDMI-1") == 0;
}

static int outputs_fetched_received(struct test_ctx *ctx)
{
    return ctx->fetched_state.outputs_count == 1 &&
           strcmp(ctx->fetched_state.outputs_name, "group1") == 0 &&
           strcmp(ctx->fetched_state.outputs_list, "DP-1 HDMI-1") == 0;
}

static int outputs_updated_received(struct test_ctx *ctx)
{
    return ctx->created.outputs_count == 2 &&
           strstr(ctx->created.outputs_list, "VGA-1") != NULL;
}

static int error_empty_name_received(struct test_ctx *ctx)
{
    return ctx->empty.error_count == 1 &&
           ctx->empty.error_code == TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_GROUP_NAME &&
           strstr(ctx->empty.error_message, "empty") != NULL;
}

static int error_single_output_received(struct test_ctx *ctx)
{
    return ctx->single.error_count == 1 &&
           ctx->single.error_code == TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_SCREEN_NUMBER;
}

static int error_duplicate_received(struct test_ctx *ctx)
{
    return ctx->dup.error_count == 1 &&
           ctx->dup.error_code == TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_GROUP_NAME &&
           strstr(ctx->dup.error_message, "already exists") != NULL;
}

static int error_emit_received(struct test_ctx *ctx)
{
    return ctx->created.error_count >= 1
        && ctx->created.error_code == TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_OUTPUT
        && strstr(ctx->created.error_message, "test error") != NULL;
}

static int list_event_received(struct test_ctx *ctx)
{
    return ctx->list_event_count == 1 && strstr(ctx->list_event_names, "group1") != NULL;
}

static const struct test_case cases[] = {
    { "manager.create_virtual_output.valid", create_valid },
    { "manager.create_virtual_output.empty_name", create_empty_name },
    { "manager.create_virtual_output.empty_outputs", create_empty_outputs },
    { "manager.create_virtual_output.duplicate_name", create_duplicate_name },
    { "manager.get_virtual_output_list", get_list },
    { "manager.get_virtual_output.existing", get_existing },
    { "event.outputs.created", outputs_created_received },
    { "event.outputs.fetched", outputs_fetched_received },
    { "event.error.empty_name", error_empty_name_received },
    { "event.error.empty_outputs", error_single_output_received },
    { "event.error.duplicate", error_duplicate_received },
    { "event.virtual_output_list", list_event_received },
    { "server.emit_outputs", emit_outputs },
    { "server.emit_error", emit_error },
    { "event.outputs.updated", outputs_updated_received },
    { "event.error.emit", error_emit_received },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->virtual_output) treeland_virtual_output_v1_destroy(ctx->virtual_output);
    if (ctx->fetched) treeland_virtual_output_v1_destroy(ctx->fetched);
    if (ctx->err_dup) treeland_virtual_output_v1_destroy(ctx->err_dup);
    if (ctx->err_single) treeland_virtual_output_v1_destroy(ctx->err_single);
    if (ctx->err_empty) treeland_virtual_output_v1_destroy(ctx->err_empty);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_virtual_output_manager_v1\n");
        test_cleanup(&ctx);
        test_destroy(&ctx);
        return 1;
    }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const int result = test_add(&ctx, cases[i].name);
        if (!cases[i].run(&ctx))
            test_fail(&ctx, result, "assertion failed");
        if (wl_display_roundtrip(ctx.display) < 0)
            test_fail(&ctx, result, "Wayland connection failed");
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    test_destroy(&ctx);
    return success ? 0 : 1;
}

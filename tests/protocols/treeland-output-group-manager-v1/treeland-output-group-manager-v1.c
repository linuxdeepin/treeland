// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-output-group-manager-v1.h"
#include "treeland-output-group-manager-unstable-v1-client-protocol.h"
#include "wlr-output-management-unstable-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void output_group_read_state(void *data);
extern void output_group_set_output_enabled(void *data);
extern void output_group_render(void *data);

#define MAX_HEADS OUTPUT_GROUP_TEST_MAX_OUTPUTS

// Minimal wlr-output-management client: it only needs to reach the head of a
// given output and push one configuration through it.
struct head_slot {
    struct test_ctx *ctx;
    struct zwlr_output_head_v1 *head;
    struct zwlr_output_mode_v1 *mode;
    char name[64];
    int enabled;
};

enum { MAX_MIRRORS = OUTPUT_GROUP_TEST_MAX_OUTPUTS };

struct group_proxy {
    struct treeland_output_group_v1 *obj;
    int name_count;
    char name[64];
    int state_count;
    char source[64];
    int mirror_count;
    char mirrors[MAX_MIRRORS][64];
    int done_count;
    int removed_count;
    int failure_count;
    uint32_t last_failure;
    char last_failure_uuid[64];
    int state_count_at;
    int failure_count_at;
};

struct test_ctx {
    struct client_connection connection;
    struct treeland_output_group_manager_v1 *manager;

    int groups_done_count;
    int group_added_count;
    char added_names[OUTPUT_GROUP_TEST_MAX_GROUPS][64];
    int group_removed_count;
    char removed_name[64];

    struct group_proxy first;
    struct group_proxy second;
    struct group_proxy invalid;
    struct group_proxy promotion;
    struct group_proxy queued;

    struct output_group_server_state server;
    char uuid[OUTPUT_GROUP_TEST_MAX_OUTPUTS][64];

    struct zwlr_output_manager_v1 *wlrManager;
    struct head_slot *headSlots[MAX_HEADS];
    int headCount;
    int headsDone;
    uint32_t wlrSerial;
    struct zwlr_output_configuration_v1 *configuration;
    int configurationSucceeded;
    int configurationFailed;

    struct test_result *results;
    int result_count;
    int result_cap;
};

/* ---- tiny result bookkeeping (same contract as the other protocol tests) ---- */

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

/* ---- events ---- */

static void parse_uuid_array(struct wl_array *array, uint32_t count, char mirrors[][64])
{
    if (!array || count == 0 || array->size == 0)
        return;

    const size_t length = array->size / count;
    const char *cursor = array->data;
    for (uint32_t i = 0; i < count && i < MAX_MIRRORS; ++i) {
        const size_t copy = length < 63 ? length : 63;
        memcpy(mirrors[i], cursor, copy);
        mirrors[i][copy] = '\0';
        cursor += length;
    }
}

static void group_name_event(void *data, struct treeland_output_group_v1 *obj, const char *name)
{
    (void)obj;
    struct group_proxy *proxy = data;
    proxy->name_count++;
    snprintf(proxy->name, sizeof(proxy->name), "%s", name ? name : "");
}

static void group_done_event(void *data, struct treeland_output_group_v1 *obj)
{
    (void)obj;
    struct group_proxy *proxy = data;
    proxy->done_count++;
}

static void group_state_event(void *data,
                              struct treeland_output_group_v1 *obj,
                              const char *source,
                              uint32_t count,
                              struct wl_array *uuids)
{
    (void)obj;
    struct group_proxy *proxy = data;
    proxy->state_count++;
    snprintf(proxy->source, sizeof(proxy->source), "%s", source ? source : "");
    proxy->mirror_count = (int)count;
    for (int i = 0; i < MAX_MIRRORS; ++i)
        proxy->mirrors[i][0] = '\0';
    parse_uuid_array(uuids, count, proxy->mirrors);
}

static void group_removed_event(void *data, struct treeland_output_group_v1 *obj)
{
    (void)obj;
    struct group_proxy *proxy = data;
    proxy->removed_count++;
}

static void group_failure_event(void *data,
                                struct treeland_output_group_v1 *obj,
                                uint32_t code,
                                const char *uuid)
{
    (void)obj;
    struct group_proxy *proxy = data;
    proxy->failure_count++;
    proxy->last_failure = code;
    snprintf(proxy->last_failure_uuid, sizeof(proxy->last_failure_uuid), "%s", uuid ? uuid : "");
}

static const struct treeland_output_group_v1_listener group_listener = {
    .name = group_name_event,
    .done = group_done_event,
    .state = group_state_event,
    .removed = group_removed_event,
    .operation_failed = group_failure_event,
};

static void manager_group_added(void *data,
                                struct treeland_output_group_manager_v1 *manager,
                                const char *name)
{
    (void)manager;
    struct test_ctx *ctx = data;
    if (ctx->group_added_count < OUTPUT_GROUP_TEST_MAX_GROUPS)
        snprintf(ctx->added_names[ctx->group_added_count], 64, "%s", name ? name : "");
    ctx->group_added_count++;
}

static void manager_groups_done(void *data, struct treeland_output_group_manager_v1 *manager)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->groups_done_count++;
}

static void manager_group_removed(void *data,
                                  struct treeland_output_group_manager_v1 *manager,
                                  const char *name)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->group_removed_count++;
    snprintf(ctx->removed_name, sizeof(ctx->removed_name), "%s", name ? name : "");
}

static const struct treeland_output_group_manager_v1_listener manager_listener = {
    .group_added = manager_group_added,
    .groups_done = manager_groups_done,
    .group_removed = manager_group_removed,
};

static void mode_size_event(void *data, struct zwlr_output_mode_v1 *mode, int32_t width, int32_t height)
{
    (void)data;
    (void)mode;
    (void)width;
    (void)height;
}

static void mode_refresh_event(void *data, struct zwlr_output_mode_v1 *mode, int32_t refresh)
{
    (void)data;
    (void)mode;
    (void)refresh;
}

static void mode_preferred_event(void *data, struct zwlr_output_mode_v1 *mode)
{
    (void)data;
    (void)mode;
}

static void mode_finished_event(void *data, struct zwlr_output_mode_v1 *mode)
{
    (void)data;
    (void)mode;
}

static const struct zwlr_output_mode_v1_listener mode_listener = {
    .size = mode_size_event,
    .refresh = mode_refresh_event,
    .preferred = mode_preferred_event,
    .finished = mode_finished_event,
};

static void head_name_event(void *data, struct zwlr_output_head_v1 *head, const char *name)
{
    (void)head;
    struct head_slot *slot = data;
    snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "");
}

static void head_description_event(void *data, struct zwlr_output_head_v1 *head, const char *description)
{
    (void)data;
    (void)head;
    (void)description;
}

static void head_physical_size_event(void *data, struct zwlr_output_head_v1 *head, int32_t w, int32_t h)
{
    (void)data;
    (void)head;
    (void)w;
    (void)h;
}

static void head_mode_event(void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *mode)
{
    (void)head;
    struct head_slot *slot = data;
    if (!slot->mode)
        slot->mode = mode;
    zwlr_output_mode_v1_add_listener(mode, &mode_listener, slot);
}

static void head_enabled_event(void *data, struct zwlr_output_head_v1 *head, int32_t enabled)
{
    (void)head;
    ((struct head_slot *)data)->enabled = enabled;
}

static void head_current_mode_event(void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *mode)
{
    (void)data;
    (void)head;
    (void)mode;
}

static void head_position_event(void *data, struct zwlr_output_head_v1 *head, int32_t x, int32_t y)
{
    (void)data;
    (void)head;
    (void)x;
    (void)y;
}

static void head_transform_event(void *data, struct zwlr_output_head_v1 *head, int32_t transform)
{
    (void)data;
    (void)head;
    (void)transform;
}

static void head_scale_event(void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale)
{
    (void)data;
    (void)head;
    (void)scale;
}

static void head_finished_event(void *data, struct zwlr_output_head_v1 *head)
{
    (void)data;
    (void)head;
}

static void head_make_event(void *data, struct zwlr_output_head_v1 *head, const char *make)
{
    (void)data;
    (void)head;
    (void)make;
}

static void head_model_event(void *data, struct zwlr_output_head_v1 *head, const char *model)
{
    (void)data;
    (void)head;
    (void)model;
}

static void head_serial_number_event(void *data, struct zwlr_output_head_v1 *head, const char *serial)
{
    (void)data;
    (void)head;
    (void)serial;
}

static void head_adaptive_sync_event(void *data, struct zwlr_output_head_v1 *head, uint32_t state)
{
    (void)data;
    (void)head;
    (void)state;
}

static const struct zwlr_output_head_v1_listener head_listener = {
    .name = head_name_event,
    .description = head_description_event,
    .physical_size = head_physical_size_event,
    .mode = head_mode_event,
    .enabled = head_enabled_event,
    .current_mode = head_current_mode_event,
    .position = head_position_event,
    .transform = head_transform_event,
    .scale = head_scale_event,
    .finished = head_finished_event,
    .make = head_make_event,
    .model = head_model_event,
    .serial_number = head_serial_number_event,
    .adaptive_sync = head_adaptive_sync_event,
};

static void wlr_manager_head(void *data, struct zwlr_output_manager_v1 *manager,
                             struct zwlr_output_head_v1 *head)
{
    (void)manager;
    struct test_ctx *ctx = data;
    if (ctx->headCount >= MAX_HEADS)
        return;

    struct head_slot *slot = calloc(1, sizeof(*slot));
    slot->ctx = ctx;
    slot->head = head;
    ctx->headSlots[ctx->headCount++] = slot;
    zwlr_output_head_v1_add_listener(head, &head_listener, slot);
}

static void wlr_manager_done(void *data, struct zwlr_output_manager_v1 *manager, uint32_t serial)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->wlrSerial = serial;
    ctx->headsDone = 1;
}

static void wlr_manager_finished(void *data, struct zwlr_output_manager_v1 *manager)
{
    (void)manager;
    (void)data;
}

static const struct zwlr_output_manager_v1_listener wlr_manager_listener = {
    .head = wlr_manager_head,
    .done = wlr_manager_done,
    .finished = wlr_manager_finished,
};

static void configuration_succeeded_event(void *data, struct zwlr_output_configuration_v1 *configuration)
{
    (void)configuration;
    ((struct test_ctx *)data)->configurationSucceeded++;
}

static void configuration_failed_event(void *data, struct zwlr_output_configuration_v1 *configuration)
{
    (void)configuration;
    ((struct test_ctx *)data)->configurationFailed++;
}

static void configuration_cancelled_event(void *data, struct zwlr_output_configuration_v1 *configuration)
{
    (void)configuration;
    ((struct test_ctx *)data)->configurationFailed++;
}

static const struct zwlr_output_configuration_v1_listener configuration_listener = {
    .succeeded = configuration_succeeded_event,
    .failed = configuration_failed_event,
    .cancelled = configuration_cancelled_event,
};

static struct head_slot *head_for_uuid(struct test_ctx *ctx, const char *uuid)
{
    const char *name = NULL;
    for (int i = 0; i < ctx->server.output_count; ++i) {
        if (strcmp(ctx->server.uuids[i], uuid) == 0)
            name = ctx->server.names[i];
    }
    if (!name)
        return NULL;

    for (int i = 0; i < ctx->headCount; ++i) {
        if (ctx->headSlots[i] && strcmp(ctx->headSlots[i]->name, name) == 0)
            return ctx->headSlots[i];
    }
    return NULL;
}

static void attach_listener(struct group_proxy *proxy)
{
    proxy->failure_count_at = -1;
    if (proxy->obj)
        treeland_output_group_v1_add_listener(proxy->obj, &group_listener, proxy);
}

static int find_output(const struct output_group_server_state *state, const char *uuid)
{
    for (int i = 0; i < state->output_count; ++i) {
        if (strcmp(state->uuids[i], uuid) == 0)
            return i;
    }
    return -1;
}

static int read_server_state(struct test_ctx *ctx)
{
    memset(&ctx->server, 0, sizeof(ctx->server));
    return invoke_on_server_thread(output_group_read_state, &ctx->server);
}

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->manager = client_bind(&ctx->connection,
                               "treeland_output_group_manager_v1",
                               &treeland_output_group_manager_v1_interface,
                               1);
    if (ctx->manager)
        treeland_output_group_manager_v1_add_listener(ctx->manager, &manager_listener, ctx);

    // The apply case needs the wlr-output-management view of the outputs.
    ctx->wlrManager = client_bind(&ctx->connection,
                                  "zwlr_output_manager_v1",
                                  &zwlr_output_manager_v1_interface,
                                  4);
    if (ctx->wlrManager)
        zwlr_output_manager_v1_add_listener(ctx->wlrManager, &wlr_manager_listener, ctx);

    return ctx->manager != NULL;
}

static int has_legacy_global(struct test_ctx *ctx)
{
    for (uint32_t i = 0; i < ctx->connection.global_count; ++i) {
        if (strcmp(ctx->connection.globals[i].interface,
                   "treeland_virtual_output_manager_v1") == 0)
            return 1;
    }
    return 0;
}

/* ---- cases ---- */

struct test_case {
    const char *name;
    int (*run)(struct test_ctx *ctx);
    int (*verify)(struct test_ctx *ctx);
};

static int bind_verify(struct test_ctx *ctx)
{
    return ctx->groups_done_count == 1 && ctx->group_added_count == 0
        && !has_legacy_global(ctx);
}

static int run_create_first(struct test_ctx *ctx)
{
    if (!read_server_state(ctx) || ctx->server.output_count != 4)
        return 0;
    for (int i = 0; i < ctx->server.output_count; ++i) {
        if (!ctx->server.uuids[i][0])
            return 0;
        snprintf(ctx->uuid[i], sizeof(ctx->uuid[i]), "%s", ctx->server.uuids[i]);
    }
    ctx->first.obj = treeland_output_group_manager_v1_create_group(ctx->manager, ctx->uuid[0]);
    attach_listener(&ctx->first);
    return ctx->first.obj != NULL;
}

static int create_first_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->first;
    return p->name_count == 1 && p->done_count == 1 && p->state_count == 1
        && p->name[0] != '\0' && strcmp(p->source, ctx->uuid[0]) == 0
        && p->mirror_count == 0 && p->failure_count == 0;
}

static int run_first_add_mirror(struct test_ctx *ctx)
{
    ctx->first.state_count_at = ctx->first.state_count;
    treeland_output_group_v1_add_output(ctx->first.obj, ctx->uuid[1]);
    treeland_output_group_v1_commit(ctx->first.obj);
    return 1;
}

static int first_add_mirror_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->first;
    if (!read_server_state(ctx))
        return 0;
    const int sourceIndex = find_output(&ctx->server, ctx->uuid[0]);
    const int mirrorIndex = find_output(&ctx->server, ctx->uuid[1]);
    return p->state_count > p->state_count_at && p->mirror_count == 1
        && strcmp(p->mirrors[0], ctx->uuid[1]) == 0
        && sourceIndex >= 0 && mirrorIndex >= 0
        && ctx->server.is_copy[mirrorIndex]
        && ctx->server.proxy_index[mirrorIndex] == sourceIndex;
}

// Second group, engaged at the same time as the first one: two groups must not
// influence each other.
static int run_create_second(struct test_ctx *ctx)
{
    ctx->second.obj = treeland_output_group_manager_v1_create_group(ctx->manager,
                                                                    ctx->uuid[2]);
    attach_listener(&ctx->second);
    return ctx->second.obj != NULL;
}

static int create_second_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->second;
    return p->name_count == 1 && p->done_count == 1 && p->state_count == 1
        && strcmp(p->name, ctx->first.name) != 0
        && strcmp(p->source, ctx->uuid[2]) == 0 && p->mirror_count == 0;
}

static int run_second_add_mirror(struct test_ctx *ctx)
{
    ctx->first.state_count_at = ctx->first.state_count;
    ctx->second.state_count_at = ctx->second.state_count;
    treeland_output_group_v1_add_output(ctx->second.obj, ctx->uuid[3]);
    treeland_output_group_v1_commit(ctx->second.obj);
    return 1;
}

static int second_add_mirror_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->second;
    if (!read_server_state(ctx))
        return 0;
    const int firstSource = find_output(&ctx->server, ctx->uuid[0]);
    const int firstMirror = find_output(&ctx->server, ctx->uuid[1]);
    const int secondSource = find_output(&ctx->server, ctx->uuid[2]);
    const int secondMirror = find_output(&ctx->server, ctx->uuid[3]);
    // The first group kept its own mirror while the second group was engaged.
    return ctx->first.state_count == ctx->first.state_count_at
        && p->state_count == p->state_count_at + 1 && p->mirror_count == 1
        && strcmp(p->mirrors[0], ctx->uuid[3]) == 0
        && ctx->server.is_copy[firstMirror]
        && ctx->server.proxy_index[firstMirror] == firstSource
        && ctx->server.is_copy[secondMirror]
        && ctx->server.proxy_index[secondMirror] == secondSource
        && ctx->server.group_count == 2;
}

static int run_dissolve_second(struct test_ctx *ctx)
{
    ctx->first.state_count_at = ctx->first.state_count;
    treeland_output_group_v1_dissolve(ctx->second.obj);
    return 1;
}

static int dissolve_second_verify(struct test_ctx *ctx)
{
    if (!read_server_state(ctx))
        return 0;
    const int firstSource = find_output(&ctx->server, ctx->uuid[0]);
    const int firstMirror = find_output(&ctx->server, ctx->uuid[1]);
    const int secondSource = find_output(&ctx->server, ctx->uuid[2]);
    const int secondMirror = find_output(&ctx->server, ctx->uuid[3]);
    return ctx->second.removed_count == 1 && ctx->group_removed_count == 1
        && strcmp(ctx->removed_name, ctx->second.name) == 0
        && ctx->first.state_count == ctx->first.state_count_at
        && ctx->server.group_count == 1
        && ctx->server.is_source[secondSource] && ctx->server.is_source[secondMirror]
        && ctx->server.is_copy[firstMirror]
        && ctx->server.proxy_index[firstMirror] == firstSource;
}

static int run_duplicate_mirror(struct test_ctx *ctx)
{
    ctx->first.state_count_at = ctx->first.state_count;
    ctx->first.failure_count_at = ctx->first.failure_count;
    treeland_output_group_v1_add_output(ctx->first.obj, ctx->uuid[1]);
    treeland_output_group_v1_commit(ctx->first.obj);
    return 1;
}

static int duplicate_mirror_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->first;
    return p->failure_count == p->failure_count_at + 1
        && p->last_failure == TREELAND_OUTPUT_GROUP_V1_FAILURE_DUPLICATE_OUTPUT
        && strcmp(p->last_failure_uuid, ctx->uuid[1]) == 0
        && p->state_count == p->state_count_at;
}

static int run_invalid_output(struct test_ctx *ctx)
{
    ctx->first.state_count_at = ctx->first.state_count;
    ctx->first.failure_count_at = ctx->first.failure_count;
    treeland_output_group_v1_set_source(ctx->first.obj, "00000000-not-an-output-000000000000");
    treeland_output_group_v1_commit(ctx->first.obj);
    return 1;
}

static int invalid_output_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->first;
    return p->failure_count == p->failure_count_at + 1
        && p->last_failure == TREELAND_OUTPUT_GROUP_V1_FAILURE_INVALID_OUTPUT
        && p->state_count == p->state_count_at
        && strcmp(p->source, ctx->uuid[0]) == 0;
}

static int run_get_invalid_name(struct test_ctx *ctx)
{
    ctx->invalid.obj = treeland_output_group_manager_v1_get_group(ctx->manager, "no-such-group");
    attach_listener(&ctx->invalid);
    return ctx->invalid.obj != NULL;
}

static int get_invalid_name_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->invalid;
    return p->name_count == 0 && p->state_count == 0 && p->done_count == 1
        && p->failure_count == 1
        && p->last_failure == TREELAND_OUTPUT_GROUP_V1_FAILURE_INVALID_NAME;
}

static int run_invalid_name_non_fatal(struct test_ctx *ctx)
{
    ctx->invalid.failure_count_at = ctx->invalid.failure_count;
    treeland_output_group_v1_set_source(ctx->invalid.obj, ctx->uuid[0]);
    treeland_output_group_v1_commit(ctx->invalid.obj);
    return 1;
}

static int invalid_name_non_fatal_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->invalid;
    // Both the queued request and the commit are rejected, and the connection
    // survives them.
    return p->failure_count == p->failure_count_at + 2
        && p->last_failure == TREELAND_OUTPUT_GROUP_V1_FAILURE_INVALID_NAME;
}

// Create a group with an empty source uuid. It must come up inert (no source,
// no mirrors) without terminating the connection; the empty-source group is
// later claimed by source_in_use via set_source.
static int run_create_empty_source(struct test_ctx *ctx)
{
    ctx->second.obj = treeland_output_group_manager_v1_create_group(ctx->manager, "");
    attach_listener(&ctx->second);
    if (!ctx->second.obj)
        return 0;
    return 1;
}

static int source_in_use_run(struct test_ctx *ctx)
{
    ctx->second.failure_count_at = ctx->second.failure_count;
    ctx->second.state_count_at = ctx->second.state_count;
    treeland_output_group_v1_set_source(ctx->second.obj, ctx->uuid[0]);
    treeland_output_group_v1_commit(ctx->second.obj);
    return 1;
}

static int source_in_use_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->second;
    return p->failure_count == p->failure_count_at + 1
        && p->last_failure == TREELAND_OUTPUT_GROUP_V1_FAILURE_OUTPUT_IN_USE
        && strcmp(p->last_failure_uuid, ctx->uuid[0]) == 0
        && p->state_count == p->state_count_at;
}

static int run_remove_mirror(struct test_ctx *ctx)
{
    ctx->first.state_count_at = ctx->first.state_count;
    treeland_output_group_v1_remove_output(ctx->first.obj, ctx->uuid[1]);
    treeland_output_group_v1_commit(ctx->first.obj);
    return 1;
}

static int remove_mirror_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->first;
    if (!read_server_state(ctx))
        return 0;
    const int mirrorIndex = find_output(&ctx->server, ctx->uuid[1]);
    return p->state_count > p->state_count_at && p->mirror_count == 0
        && strcmp(p->source, ctx->uuid[0]) == 0
        && mirrorIndex >= 0 && ctx->server.is_source[mirrorIndex];
}

static int run_second_set_source(struct test_ctx *ctx)
{
    ctx->second.state_count_at = ctx->second.state_count;
    treeland_output_group_v1_set_source(ctx->second.obj, ctx->uuid[1]);
    treeland_output_group_v1_commit(ctx->second.obj);
    return 1;
}

static int second_set_source_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->second;
    return p->state_count > p->state_count_at
        && strcmp(p->source, ctx->uuid[1]) == 0
        && read_server_state(ctx) && ctx->server.group_count == 2;
}

static int set_output_enabled(struct test_ctx *ctx, const char *uuid, int enabled)
{
    (void)ctx;

    struct output_group_output_state request = { 0 };
    snprintf(request.uuid, sizeof(request.uuid), "%s", uuid);
    request.enabled = enabled;
    return invoke_on_server_thread(output_group_set_output_enabled, &request);
}

// A group whose source disappears promotes its first available mirror, and the
// promotion is permanent.
static int run_promotion_group(struct test_ctx *ctx)
{
    ctx->promotion.obj = treeland_output_group_manager_v1_create_group(ctx->manager, ctx->uuid[2]);
    attach_listener(&ctx->promotion);
    if (!ctx->promotion.obj)
        return 0;
    treeland_output_group_v1_add_output(ctx->promotion.obj, ctx->uuid[3]);
    treeland_output_group_v1_commit(ctx->promotion.obj);
    return 1;
}

static int promotion_group_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->promotion;
    return strcmp(p->source, ctx->uuid[2]) == 0 && p->mirror_count == 1
        && strcmp(p->mirrors[0], ctx->uuid[3]) == 0;
}

static int run_promotion_disable_source(struct test_ctx *ctx)
{
    ctx->promotion.state_count_at = ctx->promotion.state_count;
    return set_output_enabled(ctx, ctx->uuid[2], 0);
}

static int promotion_disable_source_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->promotion;
    return p->state_count > p->state_count_at && p->mirror_count == 0
        && strcmp(p->source, ctx->uuid[3]) == 0
        && read_server_state(ctx)
        && ctx->server.is_source[find_output(&ctx->server, ctx->uuid[3])];
}

static int run_promotion_re_enable(struct test_ctx *ctx)
{
    ctx->promotion.state_count_at = ctx->promotion.state_count;
    return set_output_enabled(ctx, ctx->uuid[2], 1);
}

static int promotion_re_enable_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->promotion;
    if (!read_server_state(ctx))
        return 0;
    const int promoted = find_output(&ctx->server, ctx->uuid[3]);
    const int oldSource = find_output(&ctx->server, ctx->uuid[2]);
    // The promoted output stays the source; the output that came back is a
    // mirror now.
    return p->state_count > p->state_count_at && p->mirror_count == 1
        && strcmp(p->source, ctx->uuid[3]) == 0
        && strcmp(p->mirrors[0], ctx->uuid[2]) == 0
        && ctx->server.is_copy[oldSource]
        && ctx->server.proxy_index[oldSource] == promoted;
}

static int run_promotion_dissolve(struct test_ctx *ctx)
{
    treeland_output_group_v1_dissolve(ctx->promotion.obj);
    return 1;
}

static int promotion_dissolve_verify(struct test_ctx *ctx)
{
    if (!read_server_state(ctx))
        return 0;
    const int idx2 = find_output(&ctx->server, ctx->uuid[2]);
    const int idx3 = find_output(&ctx->server, ctx->uuid[3]);
    return ctx->promotion.removed_count == 1 && ctx->server.group_count == 0
        && ctx->server.is_source[idx2] && ctx->server.is_source[idx3];
}

// Free the second group and give the first group a mirror again, so the
// switched-off case can be observed on a group that is actually in copy mode.
static int run_prepare_mirror_case(struct test_ctx *ctx)
{
    treeland_output_group_v1_dissolve(ctx->second.obj);
    ctx->first.state_count_at = ctx->first.state_count;
    treeland_output_group_v1_add_output(ctx->first.obj, ctx->uuid[1]);
    treeland_output_group_v1_commit(ctx->first.obj);
    return 1;
}

static int prepare_mirror_case_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->first;
    return p->state_count > p->state_count_at && p->mirror_count == 1
        && strcmp(p->mirrors[0], ctx->uuid[1]) == 0;
}

// Switching an output off (DPMS / single-output display) must not bring it
// back to life when the compositor reconciles the groups.
static int run_mirror_off(struct test_ctx *ctx)
{
    ctx->first.state_count_at = ctx->first.state_count;
    return set_output_enabled(ctx, ctx->uuid[1], 0);
}

static int mirror_off_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->first;
    if (!read_server_state(ctx))
        return 0;
    const int mirrorIndex = find_output(&ctx->server, ctx->uuid[1]);
    return p->state_count > p->state_count_at && p->mirror_count == 0
        && strcmp(p->source, ctx->uuid[0]) == 0
        && mirrorIndex >= 0 && !ctx->server.is_enabled[mirrorIndex];
}

static int run_mirror_off_then_on(struct test_ctx *ctx)
{
    ctx->first.state_count_at = ctx->first.state_count;
    return set_output_enabled(ctx, ctx->uuid[1], 1);
}

static int mirror_off_then_on_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->first;
    if (!read_server_state(ctx))
        return 0;
    const int sourceIndex = find_output(&ctx->server, ctx->uuid[0]);
    const int mirrorIndex = find_output(&ctx->server, ctx->uuid[1]);
    return p->state_count > p->state_count_at && p->mirror_count == 1
        && strcmp(p->mirrors[0], ctx->uuid[1]) == 0
        && ctx->server.is_enabled[mirrorIndex]
        && ctx->server.is_copy[mirrorIndex]
        && ctx->server.proxy_index[mirrorIndex] == sourceIndex;
}

// Only the last set_source of one commit batch is applied; the earlier
// designation is dropped without touching its output and without reporting a
// failure for it.
static int run_set_source_sequence(struct test_ctx *ctx)
{
    ctx->queued.obj = treeland_output_group_manager_v1_create_group(ctx->manager, ctx->uuid[0]);
    attach_listener(&ctx->queued);
    if (!ctx->queued.obj)
        return 0;

    treeland_output_group_v1_set_source(ctx->queued.obj, ctx->uuid[1]);
    treeland_output_group_v1_set_source(ctx->queued.obj, ctx->uuid[2]);
    treeland_output_group_v1_commit(ctx->queued.obj);
    return 1;
}

static int set_source_sequence_verify(struct test_ctx *ctx)
{
    const struct group_proxy *p = &ctx->queued;
    if (!read_server_state(ctx))
        return 0;
    const int skippedIndex = find_output(&ctx->server, ctx->uuid[1]);
    // Initial state (source uuid0) plus the state after the commit.
    return p->state_count == 2 && p->failure_count == 0
        && p->mirror_count == 1 && strcmp(p->mirrors[0], ctx->uuid[0]) == 0
        && strcmp(p->source, ctx->uuid[2]) == 0
        && skippedIndex >= 0 && ctx->server.is_source[skippedIndex];
}

static int run_set_source_sequence_dissolve(struct test_ctx *ctx)
{
    treeland_output_group_v1_dissolve(ctx->queued.obj);
    return 1;
}

static int set_source_sequence_dissolve_verify(struct test_ctx *ctx)
{
    return ctx->queued.removed_count == 1;
}

// Reconfiguring the outputs through wlr-output-management tears copy mode down
// and re-applies it from the groups. The protocol state must end up matching
// what is actually rendered, also when the apply changes nothing about output
// availability (here: the mirror head only moves).
static int run_output_management_apply(struct test_ctx *ctx)
{
    if (!ctx->wlrManager || !ctx->headsDone || !read_server_state(ctx))
        return 0;

    struct head_slot *slot = head_for_uuid(ctx, ctx->uuid[1]);
    if (!slot)
        return 0;

    struct zwlr_output_configuration_v1 *configuration =
        zwlr_output_manager_v1_create_configuration(ctx->wlrManager, ctx->wlrSerial);
    if (!configuration)
        return 0;
    zwlr_output_configuration_v1_add_listener(configuration, &configuration_listener, ctx);

    struct zwlr_output_configuration_head_v1 *configHead =
        zwlr_output_configuration_v1_enable_head(configuration, slot->head);
    if (!configHead) {
        zwlr_output_configuration_v1_destroy(configuration);
        return 0;
    }

    // Headless outputs have no advertised fixed mode; use the custom mode the
    // compositor already runs them with and only move the head.
    zwlr_output_configuration_head_v1_set_custom_mode(configHead, 1920, 1080, 0);
    zwlr_output_configuration_head_v1_set_position(configHead, 40, 60);
    zwlr_output_configuration_head_v1_set_transform(configHead, WL_OUTPUT_TRANSFORM_NORMAL);
    zwlr_output_configuration_head_v1_set_scale(configHead, wl_fixed_from_int(1));
    zwlr_output_configuration_head_v1_set_adaptive_sync(
        configHead, ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);

    ctx->configuration = configuration;
    zwlr_output_configuration_v1_apply(configuration);
    return 1;
}

static int output_management_apply_verify(struct test_ctx *ctx)
{
    // The apply completes on an output frame; drive frames until the
    // configuration reports a result.
    for (int i = 0; i < 10 && ctx->configurationSucceeded == 0
         && ctx->configurationFailed == 0; ++i) {
        if (!invoke_on_server_thread(output_group_render, NULL))
            return 0;
        if (wl_display_roundtrip(ctx->connection.display) < 0)
            return 0;
    }

    if (ctx->configuration) {
        zwlr_output_configuration_v1_destroy(ctx->configuration);
        ctx->configuration = NULL;
    }
    if (ctx->configurationSucceeded != 1 || ctx->configurationFailed != 0) {
        fprintf(stderr, "output management apply failed: succeeded=%d failed=%d\n",
                ctx->configurationSucceeded, ctx->configurationFailed);
        return 0;
    }

    // The compositor re-applies the groups asynchronously; the last reported
    // group state must match the real output objects.
    const struct group_proxy *p = &ctx->first;
    for (int i = 0; i < 10; ++i) {
        if (!invoke_on_server_thread(output_group_render, NULL))
            return 0;
        if (wl_display_roundtrip(ctx->connection.display) < 0 || !read_server_state(ctx))
            return 0;

        const int sourceIndex = find_output(&ctx->server, ctx->uuid[0]);
        const int mirrorIndex = find_output(&ctx->server, ctx->uuid[1]);
        if (p->mirror_count == 1 && strcmp(p->mirrors[0], ctx->uuid[1]) == 0
            && ctx->server.is_copy[mirrorIndex]
            && ctx->server.proxy_index[mirrorIndex] == sourceIndex) {
            return 1;
        }
    }

    const int mirrorIndex = find_output(&ctx->server, ctx->uuid[1]);
    fprintf(stderr, "group state does not match the outputs after an apply: mirrors=%d copy=%d\n",
            p->mirror_count, mirrorIndex >= 0 ? ctx->server.is_copy[mirrorIndex] : -1);
    return 0;
}

// Disabling a mirror through wlr-output-management (the "turn this screen off"
// path of the display panel) must stick: the output stays off and drops out of
// the reported mirror list.
static int run_output_management_disable_mirror(struct test_ctx *ctx)
{
    if (!ctx->wlrManager || !read_server_state(ctx))
        return 0;

    struct head_slot *slot = head_for_uuid(ctx, ctx->uuid[1]);
    if (!slot)
        return 0;

    struct zwlr_output_configuration_v1 *configuration =
        zwlr_output_manager_v1_create_configuration(ctx->wlrManager, ctx->wlrSerial);
    if (!configuration)
        return 0;
    zwlr_output_configuration_v1_add_listener(configuration, &configuration_listener, ctx);
    zwlr_output_configuration_v1_disable_head(configuration, slot->head);

    ctx->first.state_count_at = ctx->first.state_count;
    ctx->configuration = configuration;
    zwlr_output_configuration_v1_apply(configuration);
    return 1;
}

static int output_management_disable_mirror_verify(struct test_ctx *ctx)
{
    for (int i = 0; i < 10 && ctx->configurationSucceeded == 0
         && ctx->configurationFailed == 0; ++i) {
        if (!invoke_on_server_thread(output_group_render, NULL))
            return 0;
        if (wl_display_roundtrip(ctx->connection.display) < 0)
            return 0;
    }

    if (ctx->configuration) {
        zwlr_output_configuration_v1_destroy(ctx->configuration);
        ctx->configuration = NULL;
    }
    if (ctx->configurationSucceeded != 1 || ctx->configurationFailed != 0) {
        fprintf(stderr, "disable apply failed: succeeded=%d failed=%d\n",
                ctx->configurationSucceeded, ctx->configurationFailed);
        return 0;
    }

    struct head_slot *slot = head_for_uuid(ctx, ctx->uuid[1]);
    for (int i = 0; i < 10; ++i) {
        if (!invoke_on_server_thread(output_group_render, NULL))
            return 0;
        if (wl_display_roundtrip(ctx->connection.display) < 0 || !read_server_state(ctx))
            return 0;

        const int mirrorIndex = find_output(&ctx->server, ctx->uuid[1]);
        if (mirrorIndex >= 0 && !ctx->server.is_enabled[mirrorIndex]
            && ctx->first.mirror_count == 0 && slot && slot->enabled == 0) {
            return 1;
        }
    }

    fprintf(stderr,
            "mirror disable did not stick: enabled=%d head_enabled=%d mirrors=%d\n",
            find_output(&ctx->server, ctx->uuid[1]) >= 0
                ? ctx->server.is_enabled[find_output(&ctx->server, ctx->uuid[1])]
                : -1,
            slot ? slot->enabled : -1,
            ctx->first.mirror_count);
    return 0;
}

static int run_dissolve_first(struct test_ctx *ctx)
{
    treeland_output_group_v1_dissolve(ctx->first.obj);
    return 1;
}

static int dissolve_first_verify(struct test_ctx *ctx)
{
    if (!read_server_state(ctx))
        return 0;
    const int firstSource = find_output(&ctx->server, ctx->uuid[0]);
    const int firstMirror = find_output(&ctx->server, ctx->uuid[1]);
    return ctx->first.removed_count == 1 && ctx->group_removed_count == 3
        && strcmp(ctx->removed_name, ctx->first.name) == 0
        && ctx->server.group_count == 0
        && ctx->server.is_source[firstSource] && ctx->server.is_source[firstMirror];
}

static const struct test_case cases[] = {
    { "manager.bind.groups_done", NULL, bind_verify },
    { "group.create.first", run_create_first, create_first_verify },
    { "group.first.add_output", run_first_add_mirror, first_add_mirror_verify },
    { "group.create.second", run_create_second, create_second_verify },
    { "group.second.add_output.two_groups_independent", run_second_add_mirror, second_add_mirror_verify },
    { "group.dissolve.second.other_group_unaffected", run_dissolve_second, dissolve_second_verify },
    { "group.add_output.duplicate", run_duplicate_mirror, duplicate_mirror_verify },
    { "group.set_source.invalid_output", run_invalid_output, invalid_output_verify },
    { "group.get_group.invalid_name", run_get_invalid_name, get_invalid_name_verify },
    { "group.invalid_name.non_fatal", run_invalid_name_non_fatal, invalid_name_non_fatal_verify },
    { "group.create.empty_source", run_create_empty_source, NULL },
    { "group.set_source.output_in_use", source_in_use_run, source_in_use_verify },
    { "group.remove_output", run_remove_mirror, remove_mirror_verify },
    { "group.set_source.second", run_second_set_source, second_set_source_verify },
    { "group.mirror.prepare", run_prepare_mirror_case, prepare_mirror_case_verify },
    { "group.mirror.switched_off_stays_off", run_mirror_off, mirror_off_verify },
    { "group.mirror.switched_on_again", run_mirror_off_then_on, mirror_off_then_on_verify },
    { "group.output_management.apply_state_consistent", run_output_management_apply,
      output_management_apply_verify },
    { "group.output_management.disable_mirror", run_output_management_disable_mirror,
      output_management_disable_mirror_verify },
    { "group.dissolve.first", run_dissolve_first, dissolve_first_verify },
    { "group.commit.set_source_sequence", run_set_source_sequence, set_source_sequence_verify },
    { "group.commit.set_source_sequence.dissolve", run_set_source_sequence_dissolve, set_source_sequence_dissolve_verify },
    { "group.promotion.create", run_promotion_group, promotion_group_verify },
    { "group.promotion.disable_source", run_promotion_disable_source, promotion_disable_source_verify },
    { "group.promotion.re_enable_source", run_promotion_re_enable, promotion_re_enable_verify },
    { "group.promotion.dissolve", run_promotion_dissolve, promotion_dissolve_verify },
};

// A dissolved group object must terminate the connection when it is used
// again. That needs its own connection so the main one stays usable.
static int dissolved_is_fatal(const char *socket_name)
{
    struct client_connection connection = { 0 };
    if (!client_connect(&connection, socket_name))
        return 0;

    struct treeland_output_group_manager_v1 *manager =
        client_bind(&connection,
                    "treeland_output_group_manager_v1",
                    &treeland_output_group_manager_v1_interface,
                    1);
    if (!manager) {
        client_disconnect(&connection);
        return 0;
    }

    struct output_group_server_state state = { 0 };
    if (!invoke_on_server_thread(output_group_read_state, &state)) {
        client_disconnect(&connection);
        return 0;
    }

    struct treeland_output_group_v1 *group =
        treeland_output_group_manager_v1_create_group(manager, "");
    if (!group) {
        client_disconnect(&connection);
        return 0;
    }

    treeland_output_group_v1_dissolve(group);
    if (wl_display_roundtrip(connection.display) < 0) {
        client_disconnect(&connection);
        return 0;
    }

    treeland_output_group_v1_set_source(group, state.uuids[0]);
    const int fatal = wl_display_roundtrip(connection.display) < 0;
    if (!fatal)
        treeland_output_group_v1_destroy(group);
    client_disconnect(&connection);
    return fatal;
}

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->first.obj)
        treeland_output_group_v1_destroy(ctx->first.obj);
    if (ctx->second.obj)
        treeland_output_group_v1_destroy(ctx->second.obj);
    if (ctx->invalid.obj)
        treeland_output_group_v1_destroy(ctx->invalid.obj);
    if (ctx->promotion.obj)
        treeland_output_group_v1_destroy(ctx->promotion.obj);
    if (ctx->configuration)
        zwlr_output_configuration_v1_destroy(ctx->configuration);
    if (ctx->wlrManager)
        zwlr_output_manager_v1_destroy(ctx->wlrManager);
    if (ctx->manager)
        treeland_output_group_manager_v1_destroy(ctx->manager);
    for (int i = 0; i < ctx->headCount; ++i)
        free(ctx->headSlots[i]);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx = { 0 };
    ctx.result_cap = 32;
    ctx.results = calloc((size_t)ctx.result_cap, sizeof(*ctx.results));

    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_output_group_manager_v1\n");
        test_print_results(&ctx);
        test_cleanup(&ctx);
        free(ctx.results);
        return 1;
    }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const int result = test_add(&ctx, cases[i].name);
        if (cases[i].run && !cases[i].run(&ctx)) {
            test_fail(&ctx, result, "request failed");
        } else if (wl_display_roundtrip(ctx.connection.display) < 0) {
            test_fail(&ctx, result, "Wayland connection failed");
        } else if (cases[i].verify && !cases[i].verify(&ctx)) {
            test_fail(&ctx, result, "assertion failed");
        }
    }

    {
        const int result = test_add(&ctx, "group.dissolved.fatal_error");
        if (!dissolved_is_fatal(socket_name))
            test_fail(&ctx, result, "reusing a dissolved group did not terminate the connection");
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    free(ctx.results);
    return success ? 0 : 1;
}

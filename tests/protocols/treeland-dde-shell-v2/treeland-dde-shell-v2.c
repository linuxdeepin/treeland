// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-dde-shell-v2.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "treeland-dde-shell-unstable-v2-client-protocol.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void dde_shell_v2_query_surface_state(void *data);

struct test_case {
    const char *name;
    int (*run)(struct test_ctx *ctx);
};

void test_init(struct test_ctx *ctx)
{
    ctx->result_cap = 32;
    ctx->results = calloc(ctx->result_cap, sizeof(*ctx->results));
}

void test_finish(struct test_ctx *ctx)
{
    free(ctx->results);
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

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->compositor = client_bind(&ctx->connection, "wl_compositor", &wl_compositor_interface, 1);
    ctx->seat = client_bind(&ctx->connection, "wl_seat", &wl_seat_interface, 1);
    ctx->output = client_bind(&ctx->connection, "wl_output", &wl_output_interface, 1);
    ctx->manager = client_bind(&ctx->connection, "treeland_dde_shell_manager_v2",
                                      &treeland_dde_shell_manager_v2_interface, 1);
    // Every bound resource is required by some case; a partial bind would
    // silently steer e.g. the output-anchored position test to the null path.
    return ctx->compositor && ctx->seat && ctx->output && ctx->manager != NULL;
}

static int create_shell_surface(struct test_ctx *ctx)
{
    if (!ctx->compositor)
        return 0;
    ctx->test_surface = wl_compositor_create_surface(ctx->compositor);
    ctx->shell_surface = treeland_dde_shell_manager_v2_get_shell_surface(ctx->manager,
                                                                          ctx->test_surface);
    return ctx->test_surface && ctx->shell_surface;
}

static int read_shell_surface_state(struct test_ctx *ctx, struct dde_shell_surface_v2_state *state)
{
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(dde_shell_v2_query_surface_state, state);
}

static int set_surface_role(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_role(ctx->shell_surface,
                                            TREELAND_DDE_SHELL_SURFACE_V2_ROLE_OVERLAY);
    return read_shell_surface_state(ctx, &state) && state.role_overlay && !state.role_normal;
}

static int set_surface_role_normal(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_role(ctx->shell_surface,
                                            TREELAND_DDE_SHELL_SURFACE_V2_ROLE_NORMAL);
    return read_shell_surface_state(ctx, &state) && state.role_normal && !state.role_overlay;
}

static int set_position_hint(struct test_ctx *ctx)
{
    // TODO: this asserts only the stored hint on an unmapped wl_surface; add a
    // mapped XDG fixture asserting the actual global geometry produced by
    // ShellHandler::handleDdeShellSurfaceV2Added() and Output placement, and a
    // non-origin second output to cover WOutput::position() conversion.
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_position_hint(ctx->shell_surface, ctx->output, 42, 24);
    return read_shell_surface_state(ctx, &state)
           && state.position_set && state.position_x == 42 && state.position_y == 24
           && !state.cursor_set;
}

static int set_position_hint_null_output(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_position_hint(ctx->shell_surface, NULL, 7, 9);
    return read_shell_surface_state(ctx, &state)
           && state.position_set && state.position_x == 7 && state.position_y == 9;
}

// (0,0) is a valid fixed position (output origin / global origin), it must
// register as a present hint and not be swallowed as "no request".
static int set_position_hint_zero(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_position_hint(ctx->shell_surface, NULL, 0, 0);
    return read_shell_surface_state(ctx, &state)
           && state.position_set && state.position_x == 0 && state.position_y == 0
           && !state.cursor_set;
}

static int set_cursor_placement_hint(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_cursor_placement_hint(ctx->shell_surface, 11, 5);
    return read_shell_surface_state(ctx, &state)
           && state.cursor_set && state.cursor_x == 11 && state.cursor_y == 5
           && !state.position_set;
}

static int placement_mode_switches_back(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_position_hint(ctx->shell_surface, NULL, 3, 4);
    return read_shell_surface_state(ctx, &state)
           && state.position_set && state.position_x == 3 && state.position_y == 4
           && !state.cursor_set;
}

static int set_skip_flags(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    const uint32_t all_flags = TREELAND_DDE_SHELL_SURFACE_V2_SKIP_FLAG_SWITCHER
        | TREELAND_DDE_SHELL_SURFACE_V2_SKIP_FLAG_DOCK_PREVIEW
        | TREELAND_DDE_SHELL_SURFACE_V2_SKIP_FLAG_MULTITASK_VIEW;
    treeland_dde_shell_surface_v2_set_skip_flags(ctx->shell_surface, all_flags);
    return read_shell_surface_state(ctx, &state) && state.skip_flags == (int)all_flags;
}

static int clear_skip_flags(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_skip_flags(ctx->shell_surface, 0);
    return read_shell_surface_state(ctx, &state) && state.skip_flags == 0;
}

static int set_keyboard_focus(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_accept_keyboard_focus(ctx->shell_surface, 0);
    return read_shell_surface_state(ctx, &state) && !state.accept_keyboard_focus;
}

static int shell_surface_state(struct test_ctx *ctx)
{
    (void)ctx;
    struct dde_shell_surface_v2_state state;
    // The previous set_role.normal case left the role at normal.
    return invoke_on_server_thread(dde_shell_v2_query_surface_state, &state)
           && state.role_normal && !state.accept_keyboard_focus;
}

static int destroy_shell_surface(struct test_ctx *ctx)
{
    treeland_dde_shell_surface_v2_destroy(ctx->shell_surface);
    ctx->shell_surface = NULL;
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int recreate_after_destroy(struct test_ctx *ctx)
{
    ctx->shell_surface = treeland_dde_shell_manager_v2_get_shell_surface(ctx->manager,
                                                                          ctx->test_surface);
    return ctx->shell_surface != NULL && wl_display_roundtrip(ctx->display) >= 0;
}

static int duplicate_shell_surface_error(struct test_ctx *ctx)
{
    treeland_dde_shell_manager_v2_get_shell_surface(ctx->manager, ctx->test_surface);
    // The compositor raises already_shell_surface, which kills the connection:
    // the roundtrip must fail. The client cannot reliably read back the code
    // (the server closes the connection and the IO error masks it in
    // wl_display_get_protocol_error), so the exact error path is asserted
    // server-side via the error counter.
    if (wl_display_roundtrip(ctx->display) >= 0)
        return 0;

    struct dde_shell_surface_v2_state state;
    return invoke_on_server_thread(dde_shell_v2_query_surface_state, &state)
           && state.duplicate_errors == 1;
}

static const struct test_case cases[] = {
    { "manager.get_shell_surface", create_shell_surface },
    { "shell_surface.set_role", set_surface_role },
    { "shell_surface.set_role.normal", set_surface_role_normal },
    { "shell_surface.set_position_hint", set_position_hint },
    { "shell_surface.set_position_hint.null_output", set_position_hint_null_output },
    { "shell_surface.set_position_hint.zero", set_position_hint_zero },
    { "shell_surface.set_cursor_placement_hint", set_cursor_placement_hint },
    { "shell_surface.placement_mode_switches_back", placement_mode_switches_back },
    { "shell_surface.set_skip_flags", set_skip_flags },
    { "shell_surface.clear_skip_flags", clear_skip_flags },
    { "shell_surface.set_accept_keyboard_focus", set_keyboard_focus },
    { "server.shell_surface_state", shell_surface_state },
    { "shell_surface.destroy", destroy_shell_surface },
    { "manager.recreate_after_destroy", recreate_after_destroy },
    // Must stay last: the expected protocol error tears the connection down.
    { "manager.get_shell_surface.duplicate_error", duplicate_shell_surface_error },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->shell_surface)
        treeland_dde_shell_surface_v2_destroy(ctx->shell_surface);
    if (ctx->manager)
        treeland_dde_shell_manager_v2_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}


struct role_client {
    struct treeland_dde_shell_manager_v2 *manager;
    struct treeland_dde_shell_surface_v2 *surface;
    uint32_t initial_role;
    uint32_t roles[32];
    unsigned count;
    bool overflow;
};

static void handle_role(void *data, struct treeland_dde_shell_surface_v2 *surface, uint32_t role)
{
    (void)surface;
    struct role_client *client = data;
    if (client->count >= sizeof(client->roles) / sizeof(client->roles[0])) {
        client->overflow = true;
        return;
    }
    client->roles[client->count++] = role;
}

static const struct treeland_dde_shell_surface_v2_listener role_listener = {
    .role = handle_role,
};

static int setup_role_surface(struct wl_surface *surface, void *data)
{
    struct role_client *client = data;
    client->surface = treeland_dde_shell_manager_v2_get_shell_surface(client->manager, surface);
    if (!client->surface)
        return 0;
    treeland_dde_shell_surface_v2_add_listener(client->surface, &role_listener, client);
    treeland_dde_shell_surface_v2_set_role(client->surface, client->initial_role);
    return 1;
}

// Window z layers are 0 for normal, 1 for always-on-top, and 2 for overlay.
static int check_role(struct client_connection *connection, struct role_client *client,
                      uint32_t expected, unsigned count, int z)
{
    struct dde_shell_surface_v2_state state = {0};
    if (wl_display_roundtrip(connection->display) < 0
        || !invoke_on_server_thread(dde_shell_v2_query_surface_state, &state))
        return 0;
    return !client->overflow && count > 0 && client->count == count && client->roles[count - 1] == expected
        && state.wrapper_found && state.wrapper_z == z
        && state.wrapper_always_on_top == (expected == TREELAND_DDE_SHELL_SURFACE_V2_ROLE_ALWAYS_ON_TOP);
}

extern void dde_shell_v2_user_set_always_on_top(void *data);

static int test_role_sync(const char *socket_name)
{
    struct client_connection connection = {0};
    struct xdg_toplevel_client toplevel = {0};
    struct xdg_toplevel_client child_toplevel = {0};
    struct role_client client = {.initial_role = TREELAND_DDE_SHELL_SURFACE_V2_ROLE_NORMAL};
    struct role_client child = {.initial_role = TREELAND_DDE_SHELL_SURFACE_V2_ROLE_OVERLAY};
    int success = 0;
    int enabled;
#define CHECK_ROLE_CASE(name, expression) do { \
    if (!(expression)) { fprintf(stderr, "[FAIL] %s\n", name); goto cleanup; } \
    printf("[PASS] %s\n", name); \
} while (0)
    CHECK_ROLE_CASE("role.connect", client_connect(&connection, socket_name));
    client.manager = client_bind(&connection, "treeland_dde_shell_manager_v2",
                                &treeland_dde_shell_manager_v2_interface, 2);
    CHECK_ROLE_CASE("role.bind_version_2", client.manager
        && treeland_dde_shell_manager_v2_get_version(client.manager) == 2);
    CHECK_ROLE_CASE("role.map_normal", xdg_toplevel_client_create_with_surface_setup(
        &connection, &toplevel, setup_role_surface, &client));
    CHECK_ROLE_CASE("role.initial_overlay_then_normal",
        check_role(&connection, &client, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_NORMAL, 2, 0)
        && client.roles[0] == TREELAND_DDE_SHELL_SURFACE_V2_ROLE_OVERLAY);

    treeland_dde_shell_surface_v2_set_role(client.surface, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_ALWAYS_ON_TOP);
    CHECK_ROLE_CASE("role.client_always_on_top",
        check_role(&connection, &client, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_ALWAYS_ON_TOP, 3, 1));
    treeland_dde_shell_surface_v2_set_role(client.surface, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_ALWAYS_ON_TOP);
    CHECK_ROLE_CASE("role.unchanged_no_duplicate",
        check_role(&connection, &client, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_ALWAYS_ON_TOP, 3, 1));

    enabled = 0;
    CHECK_ROLE_CASE("role.user_unpin",
        invoke_on_server_thread(dde_shell_v2_user_set_always_on_top, &enabled)
        && check_role(&connection, &client, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_NORMAL, 4, 0));
    enabled = 1;
    CHECK_ROLE_CASE("role.user_pin",
        invoke_on_server_thread(dde_shell_v2_user_set_always_on_top, &enabled)
        && check_role(&connection, &client, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_ALWAYS_ON_TOP, 5, 1));

    treeland_dde_shell_surface_v2_set_role(client.surface, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_OVERLAY);
    CHECK_ROLE_CASE("role.overlay_clears_pin",
        check_role(&connection, &client, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_OVERLAY, 6, 2));
    treeland_dde_shell_surface_v2_set_role(client.surface, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_ALWAYS_ON_TOP);
    CHECK_ROLE_CASE("role.client_pin_from_overlay",
        check_role(&connection, &client, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_ALWAYS_ON_TOP, 7, 1));
    treeland_dde_shell_surface_v2_set_role(client.surface, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_NORMAL);
    CHECK_ROLE_CASE("role.normal_clears_pin",
        check_role(&connection, &client, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_NORMAL, 8, 0));

    child.manager = client.manager;
    CHECK_ROLE_CASE("role.map_overlay_child", xdg_toplevel_client_create_with_surface_setup(
        &connection, &child_toplevel, setup_role_surface, &child));
    xdg_toplevel_set_parent(child_toplevel.toplevel, toplevel.toplevel);
    CHECK_ROLE_CASE("role.child_overlay",
        check_role(&connection, &child, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_OVERLAY, 1, 2));

    treeland_dde_shell_surface_v2_set_role(client.surface, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_ALWAYS_ON_TOP);
    CHECK_ROLE_CASE("role.parent_pin_keeps_child_overlay",
        check_role(&connection, &child, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_OVERLAY, 1, 2));
    treeland_dde_shell_surface_v2_set_role(client.surface, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_NORMAL);
    CHECK_ROLE_CASE("role.parent_unpin_keeps_child_overlay",
        check_role(&connection, &child, TREELAND_DDE_SHELL_SURFACE_V2_ROLE_OVERLAY, 1, 2));
    success = 1;
cleanup:
    if (child.surface)
        treeland_dde_shell_surface_v2_destroy(child.surface);
    xdg_toplevel_client_destroy(&child_toplevel);
    if (client.surface)
        treeland_dde_shell_surface_v2_destroy(client.surface);
    xdg_toplevel_client_destroy(&toplevel);
    if (client.manager)
        treeland_dde_shell_manager_v2_destroy(client.manager);
    if (connection.display)
        wl_display_roundtrip(connection.display);
    client_disconnect(&connection);
    return success;
#undef CHECK_ROLE_CASE
}

int protocol_test_run(const char *socket_name)
{
    if (!test_role_sync(socket_name))
        return 1;
    struct test_ctx ctx = {0};
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_dde_shell_manager_v2\n");
        test_cleanup(&ctx);
        test_finish(&ctx);
        return 1;
    }

    const size_t total_cases = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < total_cases; ++i) {
        const int result = test_add(&ctx, cases[i].name);
        if (!cases[i].run(&ctx))
            test_fail(&ctx, result, "assertion failed");
        // The final case expects the protocol error that kills the connection.
        if (i + 1 < total_cases && wl_display_roundtrip(ctx.display) < 0)
            test_fail(&ctx, result, "Wayland connection failed");
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    test_finish(&ctx);
    return success ? 0 : 1;
}

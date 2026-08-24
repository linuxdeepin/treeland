// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-personalization-manager-v1.h"
#include "server-bridge-api.h"
#include "treeland-personalization-manager-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void cursor_verfity(void *data, struct treeland_personalization_cursor_context_v1 *cursor,
                           int32_t success)
{
    struct test_ctx *ctx = data;
    if (cursor == ctx->invalid_cursor_context) {
        if (ctx->invalid_cursor_verfity_count == 0)
            ctx->invalid_cursor_verfity_first = success;
        ++ctx->invalid_cursor_verfity_count;
    } else {
        ctx->cursor_verfity = success;
        ++ctx->cursor_verfity_count;
    }
}

static void cursor_theme(void *data, struct treeland_personalization_cursor_context_v1 *cursor,
                         const char *name)
{
    struct test_ctx *ctx = data;
    (void)cursor;
    strncpy(ctx->cursor_theme, name, sizeof(ctx->cursor_theme) - 1);
    ctx->cursor_theme[sizeof(ctx->cursor_theme) - 1] = '\0';
    ++ctx->cursor_theme_count;
}

static void cursor_size(void *data, struct treeland_personalization_cursor_context_v1 *cursor,
                        uint32_t size)
{
    struct test_ctx *ctx = data;
    (void)cursor;
    ctx->cursor_size = size;
    ++ctx->cursor_size_count;
}

static const struct treeland_personalization_cursor_context_v1_listener cursor_listener = {
    .verfity = cursor_verfity,
    .theme = cursor_theme,
    .size = cursor_size,
};

static void font_event(void *data, struct treeland_personalization_font_context_v1 *font,
                       const char *font_name)
{
    struct test_ctx *ctx = data;
    (void)font;
    strncpy(ctx->font, font_name, sizeof(ctx->font) - 1);
    ctx->font[sizeof(ctx->font) - 1] = '\0';
    ++ctx->font_count;
}

static void monospace_font_event(void *data, struct treeland_personalization_font_context_v1 *font,
                                 const char *font_name)
{
    struct test_ctx *ctx = data;
    (void)font;
    strncpy(ctx->monospace_font, font_name, sizeof(ctx->monospace_font) - 1);
    ctx->monospace_font[sizeof(ctx->monospace_font) - 1] = '\0';
    ++ctx->monospace_font_count;
}

static void font_size_event(void *data, struct treeland_personalization_font_context_v1 *font,
                            uint32_t font_size)
{
    struct test_ctx *ctx = data;
    (void)font;
    ctx->font_size = font_size;
    ++ctx->font_size_count;
}

static const struct treeland_personalization_font_context_v1_listener font_listener = {
    .font = font_event,
    .monospace_font = monospace_font_event,
    .font_size = font_size_event,
};

static void round_corner_radius_event(void *data, struct treeland_personalization_appearance_context_v1 *appearance,
                                      int32_t radius)
{
    struct test_ctx *ctx = data;
    (void)appearance;
    ctx->round_corner_radius = radius;
    ++ctx->round_corner_radius_count;
}

static void icon_theme_event(void *data, struct treeland_personalization_appearance_context_v1 *appearance,
                             const char *theme_name)
{
    struct test_ctx *ctx = data;
    (void)appearance;
    strncpy(ctx->icon_theme, theme_name, sizeof(ctx->icon_theme) - 1);
    ctx->icon_theme[sizeof(ctx->icon_theme) - 1] = '\0';
    ++ctx->icon_theme_count;
}

static void active_color_event(void *data, struct treeland_personalization_appearance_context_v1 *appearance,
                               const char *active_color)
{
    struct test_ctx *ctx = data;
    (void)appearance;
    strncpy(ctx->active_color, active_color, sizeof(ctx->active_color) - 1);
    ctx->active_color[sizeof(ctx->active_color) - 1] = '\0';
    ++ctx->active_color_count;
}

static void window_opacity_event(void *data, struct treeland_personalization_appearance_context_v1 *appearance,
                                 uint32_t opacity)
{
    struct test_ctx *ctx = data;
    (void)appearance;
    ctx->window_opacity = opacity;
    ++ctx->window_opacity_count;
}

static void window_theme_type_event(void *data, struct treeland_personalization_appearance_context_v1 *appearance,
                                    uint32_t type)
{
    struct test_ctx *ctx = data;
    (void)appearance;
    ctx->window_theme_type = type;
    ++ctx->window_theme_type_count;
}

static void window_titlebar_height_event(void *data, struct treeland_personalization_appearance_context_v1 *appearance,
                                         uint32_t height)
{
    struct test_ctx *ctx = data;
    (void)appearance;
    ctx->window_titlebar_height = height;
    ++ctx->window_titlebar_height_count;
}

static const struct treeland_personalization_appearance_context_v1_listener appearance_listener = {
    .round_corner_radius = round_corner_radius_event,
    .icon_theme = icon_theme_event,
    .active_color = active_color_event,
    .window_opacity = window_opacity_event,
    .window_theme_type = window_theme_type_event,
    .window_titlebar_height = window_titlebar_height_event,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->compositor = client_bind(&ctx->connection, "wl_compositor", &wl_compositor_interface, 1);

    ctx->manager = client_bind(&ctx->connection, "treeland_personalization_manager_v1",
                                      &treeland_personalization_manager_v1_interface, 1);
    return ctx->manager != NULL;
}

static int create_window_context(struct test_ctx *ctx)
{
    if (!ctx->compositor)
        return 0;
    ctx->surface = wl_compositor_create_surface(ctx->compositor);
    ctx->window_context = treeland_personalization_manager_v1_get_window_context(ctx->manager, ctx->surface);
    return ctx->surface != NULL && ctx->window_context != NULL;
}

static int create_cursor_context(struct test_ctx *ctx)
{
    ctx->cursor_context = treeland_personalization_manager_v1_get_cursor_context(ctx->manager);
    if (ctx->cursor_context)
        treeland_personalization_cursor_context_v1_add_listener(ctx->cursor_context, &cursor_listener, ctx);
    return ctx->cursor_context != NULL;
}

static int create_invalid_cursor_context(struct test_ctx *ctx)
{
    ctx->invalid_cursor_context = treeland_personalization_manager_v1_get_cursor_context(ctx->manager);
    if (ctx->invalid_cursor_context)
        treeland_personalization_cursor_context_v1_add_listener(ctx->invalid_cursor_context, &cursor_listener, ctx);
    return ctx->invalid_cursor_context != NULL;
}

static int create_font_context(struct test_ctx *ctx)
{
    ctx->font_context = treeland_personalization_manager_v1_get_font_context(ctx->manager);
    if (ctx->font_context)
        treeland_personalization_font_context_v1_add_listener(ctx->font_context, &font_listener, ctx);
    return ctx->font_context != NULL;
}

static int create_appearance_context(struct test_ctx *ctx)
{
    ctx->appearance_context = treeland_personalization_manager_v1_get_appearance_context(ctx->manager);
    if (ctx->appearance_context)
        treeland_personalization_appearance_context_v1_add_listener(ctx->appearance_context, &appearance_listener, ctx);
    return ctx->appearance_context != NULL;
}

static int set_blend_mode(struct test_ctx *ctx)
{
    treeland_personalization_window_context_v1_set_blend_mode(ctx->window_context,
                                                              TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_BLEND_MODE_WALLPAPER);
    return 1;
}

static int set_window_corner_radius(struct test_ctx *ctx)
{
    treeland_personalization_window_context_v1_set_round_corner_radius(ctx->window_context, 12);
    return 1;
}

static int set_shadow(struct test_ctx *ctx)
{
    treeland_personalization_window_context_v1_set_shadow(ctx->window_context, 8, 2, 3, 10, 20, 30, 40);
    return 1;
}

static int set_border(struct test_ctx *ctx)
{
    treeland_personalization_window_context_v1_set_border(ctx->window_context, 2, 100, 150, 200, 255);
    return 1;
}

static int set_titlebar_disable(struct test_ctx *ctx)
{
    treeland_personalization_window_context_v1_set_titlebar(ctx->window_context,
                                                            TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_ENABLE_MODE_DISABLE);
    return 1;
}

static int set_titlebar_enable(struct test_ctx *ctx)
{
    treeland_personalization_window_context_v1_set_titlebar(ctx->window_context,
                                                            TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_ENABLE_MODE_ENABLE);
    return 1;
}

static int verify_blend_mode(struct test_ctx *ctx)
{
    (void)ctx;
    struct window_context_state state;
    if (!invoke_on_server_thread(personalization_window_state, &state))
        return 0;
    return state.background_type == TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_BLEND_MODE_WALLPAPER;
}

static int verify_corner_radius(struct test_ctx *ctx)
{
    (void)ctx;
    struct window_context_state state;
    if (!invoke_on_server_thread(personalization_window_state, &state))
        return 0;
    return state.corner_radius == 12;
}

static int verify_shadow(struct test_ctx *ctx)
{
    (void)ctx;
    struct window_context_state state;
    if (!invoke_on_server_thread(personalization_window_state, &state))
        return 0;
    return state.shadow_radius == 8 && state.shadow_offset_x == 2 && state.shadow_offset_y == 3
        && state.shadow_red == 10 && state.shadow_green == 20 && state.shadow_blue == 30
        && state.shadow_alpha == 40;
}

static int verify_border(struct test_ctx *ctx)
{
    (void)ctx;
    struct window_context_state state;
    if (!invoke_on_server_thread(personalization_window_state, &state))
        return 0;
    return state.border_width == 2 && state.border_red == 100 && state.border_green == 150
        && state.border_blue == 200 && state.border_alpha == 255;
}

static int verify_no_titlebar(struct test_ctx *ctx)
{
    (void)ctx;
    struct window_context_state state;
    if (!invoke_on_server_thread(personalization_window_state, &state))
        return 0;
    return state.no_titlebar == 1;
}

static int verify_titlebar_enabled(struct test_ctx *ctx)
{
    (void)ctx;
    struct window_context_state state;
    if (!invoke_on_server_thread(personalization_window_state, &state))
        return 0;
    return state.no_titlebar == 0;
}

static int cursor_set_theme(struct test_ctx *ctx)
{
    treeland_personalization_cursor_context_v1_set_theme(ctx->cursor_context, "test-theme");
    return 1;
}

static int cursor_get_theme(struct test_ctx *ctx)
{
    treeland_personalization_cursor_context_v1_get_theme(ctx->cursor_context);
    return 1;
}

static int cursor_theme_received(struct test_ctx *ctx)
{
    return ctx->cursor_theme_count == 1 && strcmp(ctx->cursor_theme, "test-theme") == 0;
}

static int cursor_set_size(struct test_ctx *ctx)
{
    treeland_personalization_cursor_context_v1_set_size(ctx->cursor_context, 24);
    return 1;
}

static int cursor_get_size(struct test_ctx *ctx)
{
    treeland_personalization_cursor_context_v1_get_size(ctx->cursor_context);
    return 1;
}

static int cursor_size_received(struct test_ctx *ctx)
{
    return ctx->cursor_size_count == 1 && ctx->cursor_size == 24;
}

static int cursor_commit(struct test_ctx *ctx)
{
    treeland_personalization_cursor_context_v1_commit(ctx->cursor_context);
    return 1;
}

static int cursor_verfity_received(struct test_ctx *ctx)
{
    return ctx->cursor_verfity_count == 1 && ctx->cursor_verfity == 1;
}

static int invalid_cursor_commit(struct test_ctx *ctx)
{
    treeland_personalization_cursor_context_v1_set_theme(ctx->invalid_cursor_context, "");
    treeland_personalization_cursor_context_v1_commit(ctx->invalid_cursor_context);
    return 1;
}

static int invalid_cursor_verfity_received(struct test_ctx *ctx)
{
    return ctx->invalid_cursor_verfity_count >= 1 && ctx->invalid_cursor_verfity_first == 0;
}

static int font_set_size(struct test_ctx *ctx)
{
    treeland_personalization_font_context_v1_set_font_size(ctx->font_context, 37);
    return 1;
}

static int font_size_received(struct test_ctx *ctx)
{
    return ctx->font_size_count >= 2 && ctx->font_size == 37;
}

static int font_get_size(struct test_ctx *ctx)
{
    treeland_personalization_font_context_v1_get_font_size(ctx->font_context);
    return 1;
}

static int font_size_echo_received(struct test_ctx *ctx)
{
    return ctx->font_size_count >= 3 && ctx->font_size == 37;
}

static int font_set_font(struct test_ctx *ctx)
{
    treeland_personalization_font_context_v1_set_font(ctx->font_context, "TestFont");
    return 1;
}

static int font_received(struct test_ctx *ctx)
{
    return ctx->font_count >= 2 && strcmp(ctx->font, "TestFont") == 0;
}

static int font_set_monospace_font(struct test_ctx *ctx)
{
    treeland_personalization_font_context_v1_set_monospace_font(ctx->font_context, "TestMonoFont");
    return 1;
}

static int font_get_font(struct test_ctx *ctx)
{
    treeland_personalization_font_context_v1_get_font(ctx->font_context);
    return 1;
}

static int font_echo_received(struct test_ctx *ctx)
{
    return ctx->font_count >= 3 && strcmp(ctx->font, "TestFont") == 0;
}

static int font_get_monospace_font(struct test_ctx *ctx)
{
    treeland_personalization_font_context_v1_get_monospace_font(ctx->font_context);
    return 1;
}

static int monospace_font_echo_received(struct test_ctx *ctx)
{
    return ctx->monospace_font_count >= 3 && strcmp(ctx->monospace_font, "TestMonoFont") == 0;
}

static int monospace_font_received(struct test_ctx *ctx)
{
    return ctx->monospace_font_count >= 2 && strcmp(ctx->monospace_font, "TestMonoFont") == 0;
}

static int appearance_set_radius(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_set_round_corner_radius(ctx->appearance_context, 8);
    return 1;
}

static int appearance_radius_received(struct test_ctx *ctx)
{
    return ctx->round_corner_radius_count >= 2 && ctx->round_corner_radius == 8;
}

static int appearance_set_icon_theme(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_set_icon_theme(ctx->appearance_context, "test-icons");
    return 1;
}

static int appearance_icon_theme_received(struct test_ctx *ctx)
{
    return ctx->icon_theme_count >= 2 && strcmp(ctx->icon_theme, "test-icons") == 0;
}

static int appearance_set_active_color(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_set_active_color(ctx->appearance_context, "#112233");
    return 1;
}

static int appearance_active_color_received(struct test_ctx *ctx)
{
    return ctx->active_color_count >= 2 && strcmp(ctx->active_color, "#112233") == 0;
}

static int appearance_set_opacity(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_set_window_opacity(ctx->appearance_context, 150);
    return 1;
}

static int appearance_opacity_received(struct test_ctx *ctx)
{
    return ctx->window_opacity_count >= 2 && ctx->window_opacity == 150;
}

static int appearance_set_theme_type(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_set_window_theme_type(
        ctx->appearance_context, TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_DARK);
    return 1;
}

static int appearance_theme_type_received(struct test_ctx *ctx)
{
    return ctx->window_theme_type_count >= 2
        && ctx->window_theme_type == TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_DARK;
}

static int appearance_set_titlebar_height(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_set_window_titlebar_height(ctx->appearance_context, 36);
    return 1;
}

static int appearance_titlebar_height_received(struct test_ctx *ctx)
{
    return ctx->window_titlebar_height_count >= 2 && ctx->window_titlebar_height == 36;
}

static int appearance_get_radius(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_get_round_corner_radius(ctx->appearance_context);
    return 1;
}

static int appearance_radius_echo_received(struct test_ctx *ctx)
{
    return ctx->round_corner_radius_count >= 3 && ctx->round_corner_radius == 8;
}

static int appearance_get_icon_theme(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_get_icon_theme(ctx->appearance_context);
    return 1;
}

static int appearance_icon_theme_echo_received(struct test_ctx *ctx)
{
    return ctx->icon_theme_count >= 3 && strcmp(ctx->icon_theme, "test-icons") == 0;
}

static int appearance_get_active_color(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_get_active_color(ctx->appearance_context);
    return 1;
}

static int appearance_active_color_echo_received(struct test_ctx *ctx)
{
    return ctx->active_color_count >= 3 && strcmp(ctx->active_color, "#112233") == 0;
}

static int appearance_get_opacity(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_get_window_opacity(ctx->appearance_context);
    return 1;
}

static int appearance_opacity_echo_received(struct test_ctx *ctx)
{
    return ctx->window_opacity_count >= 3 && ctx->window_opacity == 150;
}

static int appearance_get_theme_type(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_get_window_theme_type(ctx->appearance_context);
    return 1;
}

static int appearance_theme_type_echo_received(struct test_ctx *ctx)
{
    return ctx->window_theme_type_count >= 3
        && ctx->window_theme_type == TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_DARK;
}

static int appearance_get_titlebar_height(struct test_ctx *ctx)
{
    treeland_personalization_appearance_context_v1_get_window_titlebar_height(ctx->appearance_context);
    return 1;
}

static int appearance_titlebar_height_echo_received(struct test_ctx *ctx)
{
    return ctx->window_titlebar_height_count >= 3 && ctx->window_titlebar_height == 36;
}

static const struct test_case cases[] = {
    { "manager.get_window_context", create_window_context },
    { "window.set_blend_mode", set_blend_mode },
    { "window.state.blend_mode", verify_blend_mode },
    { "window.set_round_corner_radius", set_window_corner_radius },
    { "window.state.corner_radius", verify_corner_radius },
    { "window.set_shadow", set_shadow },
    { "window.state.shadow", verify_shadow },
    { "window.set_border", set_border },
    { "window.state.border", verify_border },
    { "window.set_titlebar.disable", set_titlebar_disable },
    { "window.state.no_titlebar", verify_no_titlebar },
    { "window.set_titlebar.enable", set_titlebar_enable },
    { "window.state.titlebar_enabled", verify_titlebar_enabled },
    { "manager.get_cursor_context", create_cursor_context },
    { "cursor.set_theme", cursor_set_theme },
    { "cursor.get_theme", cursor_get_theme },
    { "cursor.event.theme", cursor_theme_received },
    { "cursor.set_size", cursor_set_size },
    { "cursor.get_size", cursor_get_size },
    { "cursor.event.size", cursor_size_received },
    { "cursor.commit", cursor_commit },
    { "cursor.event.verfity", cursor_verfity_received },
    { "manager.get_font_context", create_font_context },
    { "font.set_font_size", font_set_size },
    { "font.event.font_size", font_size_received },
    { "font.set_font", font_set_font },
    { "font.event.font", font_received },
    { "font.get_font", font_get_font },
    { "font.event.font_echo", font_echo_received },
    { "font.set_monospace_font", font_set_monospace_font },
    { "font.event.monospace_font", monospace_font_received },
    { "font.get_monospace_font", font_get_monospace_font },
    { "font.event.monospace_font_echo", monospace_font_echo_received },
    { "font.get_font_size", font_get_size },
    { "font.event.font_size_echo", font_size_echo_received },
    { "manager.get_appearance_context", create_appearance_context },
    { "appearance.set_round_corner_radius", appearance_set_radius },
    { "appearance.event.round_corner_radius", appearance_radius_received },
    { "appearance.set_icon_theme", appearance_set_icon_theme },
    { "appearance.event.icon_theme", appearance_icon_theme_received },
    { "appearance.set_active_color", appearance_set_active_color },
    { "appearance.event.active_color", appearance_active_color_received },
    { "appearance.set_window_opacity", appearance_set_opacity },
    { "appearance.event.window_opacity", appearance_opacity_received },
    { "appearance.set_window_theme_type", appearance_set_theme_type },
    { "appearance.event.window_theme_type", appearance_theme_type_received },
    { "appearance.set_window_titlebar_height", appearance_set_titlebar_height },
    { "appearance.event.window_titlebar_height", appearance_titlebar_height_received },
    { "appearance.get_round_corner_radius", appearance_get_radius },
    { "appearance.event.round_corner_radius_echo", appearance_radius_echo_received },
    { "appearance.get_icon_theme", appearance_get_icon_theme },
    { "appearance.event.icon_theme_echo", appearance_icon_theme_echo_received },
    { "appearance.get_active_color", appearance_get_active_color },
    { "appearance.event.active_color_echo", appearance_active_color_echo_received },
    { "appearance.get_window_opacity", appearance_get_opacity },
    { "appearance.event.window_opacity_echo", appearance_opacity_echo_received },
    { "appearance.get_window_theme_type", appearance_get_theme_type },
    { "appearance.event.window_theme_type_echo", appearance_theme_type_echo_received },
    { "appearance.get_window_titlebar_height", appearance_get_titlebar_height },
    { "appearance.event.window_titlebar_height_echo", appearance_titlebar_height_echo_received },
    { "manager.get_cursor_context.invalid", create_invalid_cursor_context },
    { "cursor_invalid.commit", invalid_cursor_commit },
    { "cursor_invalid.event.verfity_fail", invalid_cursor_verfity_received },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->display) {
        invoke_on_server_thread(personalization_restore_config, NULL);

        wl_display_roundtrip(ctx->display);
    }
    if (ctx->window_context) treeland_personalization_window_context_v1_destroy(ctx->window_context);
    if (ctx->cursor_context) treeland_personalization_cursor_context_v1_destroy(ctx->cursor_context);
    if (ctx->invalid_cursor_context) treeland_personalization_cursor_context_v1_destroy(ctx->invalid_cursor_context);
    if (ctx->font_context) treeland_personalization_font_context_v1_destroy(ctx->font_context);
    if (ctx->appearance_context) treeland_personalization_appearance_context_v1_destroy(ctx->appearance_context);
    if (ctx->surface) wl_surface_destroy(ctx->surface);

    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_personalization_manager_v1\n");
        test_cleanup(&ctx);
        test_destroy(&ctx);
        return 1;
    }

    int snapshot_valid = 0;
    if (!invoke_on_server_thread(personalization_snapshot_config, &snapshot_valid)
        || !snapshot_valid) {
        fprintf(stderr, "failed to snapshot personalization configuration\n");
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

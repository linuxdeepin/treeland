// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef TREELAND_PERSONALIZATION_MANAGER_V1_TEST_H
#define TREELAND_PERSONALIZATION_MANAGER_V1_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);

struct window_context_state {
    int32_t background_type;
    int32_t corner_radius;
    int32_t shadow_radius;
    int32_t shadow_offset_x;
    int32_t shadow_offset_y;
    int32_t shadow_red;
    int32_t shadow_green;
    int32_t shadow_blue;
    int32_t shadow_alpha;
    int32_t border_width;
    int32_t border_red;
    int32_t border_green;
    int32_t border_blue;
    int32_t border_alpha;
    int32_t no_titlebar;
};

void personalization_window_state(void *data);
void personalization_snapshot_config(void *data);
void personalization_restore_config(void *data);

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

    struct treeland_personalization_manager_v1 *manager;
    struct treeland_personalization_window_context_v1 *window_context;
    struct treeland_personalization_cursor_context_v1 *cursor_context;
    struct treeland_personalization_cursor_context_v1 *invalid_cursor_context;
    struct treeland_personalization_font_context_v1 *font_context;
    struct treeland_personalization_appearance_context_v1 *appearance_context;
    struct wl_surface *surface;

    char     cursor_theme[128];
    int      cursor_theme_count;
    uint32_t cursor_size;
    int      cursor_size_count;
    int32_t  cursor_verfity;
    int      cursor_verfity_count;

    int32_t invalid_cursor_verfity_first;
    int     invalid_cursor_verfity_count;

    char     font[128];
    int      font_count;
    char     monospace_font[128];
    int      monospace_font_count;
    uint32_t font_size;
    int      font_size_count;

    int32_t  round_corner_radius;
    int      round_corner_radius_count;
    char     icon_theme[128];
    int      icon_theme_count;
    char     active_color[128];
    int      active_color_count;
    uint32_t window_opacity;
    int      window_opacity_count;
    uint32_t window_theme_type;
    int      window_theme_type_count;
    uint32_t window_titlebar_height;
    int      window_titlebar_height_count;

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

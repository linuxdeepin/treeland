// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "appearance-isolation.h"
#include "client-connection.h"
#include "treeland-appearance-unstable-v1-client-protocol.h"
#include "treeland-appearance-manager-unstable-v1-client-protocol.h"

struct appearance_state { unsigned font_events; uint32_t font_size; };

static void cursor_theme(void *data, struct treeland_appearance_v1 *appearance, const char *name)
{ (void)data; (void)appearance; (void)name; }
static void cursor_size(void *data, struct treeland_appearance_v1 *appearance, uint32_t width, uint32_t height)
{ (void)data; (void)appearance; (void)width; (void)height; }
static void font(void *data, struct treeland_appearance_v1 *appearance, const char *name)
{ (void)data; (void)appearance; (void)name; }
static void monospace_font(void *data, struct treeland_appearance_v1 *appearance, const char *name)
{ (void)data; (void)appearance; (void)name; }
static void font_size(void *data, struct treeland_appearance_v1 *appearance, uint32_t size)
{ (void)appearance; struct appearance_state *state = data; state->font_events++; state->font_size = size; }
static void icon_theme(void *data, struct treeland_appearance_v1 *appearance, const char *name)
{ (void)data; (void)appearance; (void)name; }
static void accent_color(void *data, struct treeland_appearance_v1 *appearance, uint32_t r, uint32_t g, uint32_t b)
{ (void)data; (void)appearance; (void)r; (void)g; (void)b; }
static void window_opacity(void *data, struct treeland_appearance_v1 *appearance, wl_fixed_t opacity)
{ (void)data; (void)appearance; (void)opacity; }
static void color_scheme(void *data, struct treeland_appearance_v1 *appearance, uint32_t scheme)
{ (void)data; (void)appearance; (void)scheme; }
static void titlebar_height(void *data, struct treeland_appearance_v1 *appearance, uint32_t height)
{ (void)data; (void)appearance; (void)height; }
static void corner_radius(void *data, struct treeland_appearance_v1 *appearance, uint32_t radius)
{ (void)data; (void)appearance; (void)radius; }

static const struct treeland_appearance_v1_listener appearance_listener = {
    cursor_theme, cursor_size, font, monospace_font, font_size, icon_theme,
    accent_color, window_opacity, color_scheme, titlebar_height, corner_radius,
};

int session_test_appearance_isolation(struct client_connection *a, struct client_connection *b)
{
    struct appearance_state a_state = {0}, b_state = {0};
    struct treeland_appearance_v1 *a_events = client_bind(a, "treeland_appearance_v1", &treeland_appearance_v1_interface, 1);
    struct treeland_appearance_v1 *b_events = client_bind(b, "treeland_appearance_v1", &treeland_appearance_v1_interface, 1);
    struct treeland_appearance_manager_v1 *a_manager = client_bind(a, "treeland_appearance_manager_v1", &treeland_appearance_manager_v1_interface, 1);
    struct treeland_appearance_manager_v1 *b_manager = client_bind(b, "treeland_appearance_manager_v1", &treeland_appearance_manager_v1_interface, 1);
    int result = 1;
    if (!a_events || !b_events || !a_manager || !b_manager)
        goto cleanup;
    treeland_appearance_v1_add_listener(a_events, &appearance_listener, &a_state);
    treeland_appearance_v1_add_listener(b_events, &appearance_listener, &b_state);
    if (wl_display_roundtrip(a->display) < 0 || wl_display_roundtrip(b->display) < 0
        || !a_state.font_events || !b_state.font_events)
        goto cleanup;
    const unsigned b_before = b_state.font_events;
    const uint32_t b_size = b_state.font_size;
    treeland_appearance_manager_v1_set_font_size(a_manager, 17);
    if (wl_display_roundtrip(a->display) < 0 || wl_display_roundtrip(b->display) < 0
        || a_state.font_size != 17 || b_state.font_events != b_before || b_state.font_size != b_size)
        goto cleanup;
    const unsigned a_before = a_state.font_events;
    treeland_appearance_manager_v1_set_font_size(b_manager, 22);
    if (wl_display_roundtrip(b->display) < 0 || wl_display_roundtrip(a->display) < 0
        || b_state.font_size != 22 || a_state.font_events != a_before || a_state.font_size != 17)
        goto cleanup;
    result = 0;
cleanup:
    if (a_manager) treeland_appearance_manager_v1_destroy(a_manager);
    if (b_manager) treeland_appearance_manager_v1_destroy(b_manager);
    if (a_events) treeland_appearance_v1_destroy(a_events);
    if (b_events) treeland_appearance_v1_destroy(b_events);
    return result;
}

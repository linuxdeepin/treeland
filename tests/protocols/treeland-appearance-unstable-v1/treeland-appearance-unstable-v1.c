// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "treeland-appearance-unstable-v1-client-protocol.h"
#include "treeland-appearance-manager-unstable-v1-client-protocol.h"

#include <stdio.h>

struct appearance_state {
    unsigned initial_color_scheme_events;
    unsigned color_scheme;
};

static void cursor_theme(void *data, struct treeland_appearance_v1 *appearance, const char *name)
{ (void)data; (void)appearance; (void)name; }
static void cursor_size(void *data, struct treeland_appearance_v1 *appearance, uint32_t width, uint32_t height)
{ (void)data; (void)appearance; (void)width; (void)height; }
static void font(void *data, struct treeland_appearance_v1 *appearance, const char *name)
{ (void)data; (void)appearance; (void)name; }
static void monospace_font(void *data, struct treeland_appearance_v1 *appearance, const char *name)
{ (void)data; (void)appearance; (void)name; }
static void font_size(void *data, struct treeland_appearance_v1 *appearance, uint32_t size)
{ (void)data; (void)appearance; (void)size; }
static void icon_theme(void *data, struct treeland_appearance_v1 *appearance, const char *name)
{ (void)data; (void)appearance; (void)name; }
static void accent_color(void *data, struct treeland_appearance_v1 *appearance, uint32_t r, uint32_t g, uint32_t b)
{ (void)data; (void)appearance; (void)r; (void)g; (void)b; }
static void window_opacity(void *data, struct treeland_appearance_v1 *appearance, wl_fixed_t opacity)
{ (void)data; (void)appearance; (void)opacity; }
static void color_scheme(void *data, struct treeland_appearance_v1 *appearance, uint32_t scheme)
{ (void)appearance; struct appearance_state *state = data; state->initial_color_scheme_events++; state->color_scheme = scheme; }
static void titlebar_height(void *data, struct treeland_appearance_v1 *appearance, uint32_t height)
{ (void)data; (void)appearance; (void)height; }
static void corner_radius(void *data, struct treeland_appearance_v1 *appearance, uint32_t radius)
{ (void)data; (void)appearance; (void)radius; }

static const struct treeland_appearance_v1_listener appearance_listener = {
    cursor_theme, cursor_size, font, monospace_font, font_size, icon_theme,
    accent_color, window_opacity, color_scheme, titlebar_height, corner_radius,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct appearance_state state = { 0 };
    if (!client_connect(&connection, socket_name))
        return 1;
    struct treeland_appearance_v1 *appearance = client_bind(&connection,
        "treeland_appearance_v1", &treeland_appearance_v1_interface, 1);
    struct treeland_appearance_manager_v1 *manager = client_bind(&connection,
        "treeland_appearance_manager_v1", &treeland_appearance_manager_v1_interface, 1);
    if (!appearance || !manager) {
        fprintf(stderr, "appearance: missing global\n");
        client_disconnect(&connection);
        return 1;
    }
    treeland_appearance_v1_add_listener(appearance, &appearance_listener, &state);
    if (wl_display_roundtrip(connection.display) < 0 || !state.initial_color_scheme_events) {
        fprintf(stderr, "appearance: initial state was not sent\n");
        return 1;
    }
    const unsigned before = state.initial_color_scheme_events;
    treeland_appearance_manager_v1_set_color_scheme(manager,
        TREELAND_APPEARANCE_MANAGER_V1_COLOR_SCHEME_DARK);
    if (wl_display_roundtrip(connection.display) < 0 || state.initial_color_scheme_events <= before
        || state.color_scheme != TREELAND_APPEARANCE_V1_COLOR_SCHEME_DARK) {
        fprintf(stderr, "appearance: manager update was not observed\n");
        return 1;
    }
    treeland_appearance_manager_v1_destroy(manager);
    treeland_appearance_v1_destroy(appearance);
    client_disconnect(&connection);
    return 0;
}

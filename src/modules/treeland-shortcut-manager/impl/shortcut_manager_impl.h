// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>

struct treeland_shortcut_manager_v1
{
    struct wl_event_loop *event_loop;
    struct wl_global *global;
    struct wl_list contexts;
    struct wl_resource *client;

    struct wl_listener display_destroy;

    struct
    {
        struct wl_signal context;
        struct wl_signal destroy;
    } events;

    void *data;
};

struct treeland_shortcut_context_v1
{
    struct treeland_shortcut_manager_v1 *manager;
    char *key;
    struct wl_list link;
    struct wl_resource *resource;

    struct
    {
        struct wl_signal destroy;
    } events;
};

void shortcut_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

struct treeland_shortcut_manager_v1 *treeland_shortcut_manager_v1_create(
    struct wl_display *display);

void treeland_shortcut_context_v1_destroy(struct treeland_shortcut_context_v1 *context);

void treeland_shortcut_context_v1_send_shortcut(struct treeland_shortcut_context_v1 *context);
void treeland_shortcut_context_v1_send_register_failed(
    struct treeland_shortcut_context_v1 *context);

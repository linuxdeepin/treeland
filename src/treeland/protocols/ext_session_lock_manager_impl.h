// Copyright (C) 2024 ssk-wh <fanpengcheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "ext-session-lock-server-protocol.h"

struct ext_session_lock_manager_v1
{
    struct wl_event_loop *event_loop;
    struct wl_global *global;
    struct wl_list contexts;
    struct wl_resource *client;
    struct wl_listener display_destroy;

    struct
    {
        struct wl_signal lock;
        struct wl_signal destroy;
    } events;
};

struct ext_session_lock_v1
{
    struct wl_resource *resource;
    uint32_t id;
    struct wl_list contexts;

    struct
    {
        struct wl_signal get_lock_surface;
        struct wl_signal unlock_and_destroy;
        struct wl_signal destroy;
    } events;
};

struct ext_session_lock_surface_v1
{
    struct wl_resource *resource;
    struct wl_resource *surface;
    struct wl_resource *output;
    uint32_t id;

    struct
    {
        struct wl_signal ack_configure;
        struct wl_signal destroy;
    } events;
};

void ext_session_lock_v1_destroy(struct ext_session_lock_v1 *context);
void ext_session_lock_surface_v1_destroy(struct ext_session_lock_surface_v1 *context);
struct ext_session_lock_manager_v1 *ext_session_lock_manager_v1_create(struct wl_display *display);

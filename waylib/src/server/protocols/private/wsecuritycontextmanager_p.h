// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QPointer>

#include <wayland-server-core.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSocket;

// This is waylib's implementation-private copy of wlroots' security-context
// types. It intentionally differs from the vendored wlroots header: the
// manager also emits events.new_client.
struct wlr_security_context_manager_v1 {
    struct wl_global *global;

    struct {
        struct wl_signal destroy;
        struct wl_signal commit; // struct wlr_security_context_v1_commit_event
        struct wl_signal new_client; // struct wl_client
    } events;

    void *data;

    struct wl_list contexts; // wlr_security_context_v1.link
    struct wl_listener display_destroy;
};

struct wlr_security_context_v1_state {
    char *sandbox_engine; // may be NULL
    char *app_id; // may be NULL
    char *instance_id; // may be NULL
};

struct wlr_security_context_v1_commit_event {
    const struct wlr_security_context_v1_state *state;
    // Client which created the security context
    struct wl_client *parent_client;
};

struct wlr_security_context_v1 {
    struct wlr_security_context_manager_v1 *manager;
    struct wlr_security_context_v1_state state;
    struct wl_list link; // wlr_security_context_manager_v1.contexts
    int listen_fd, close_fd;
    struct wl_event_source *listen_source, *close_source;
    QPointer<WSocket> socket;
};

struct wlr_security_context_v1_client {
    struct wlr_security_context_v1_state state;
    struct wl_listener destroy;
};

struct wlr_security_context_manager_v1 *wlr_security_context_manager_v1_create(
    struct wl_display *display);
const struct wlr_security_context_v1_state *wlr_security_context_manager_v1_lookup_client(
    struct wlr_security_context_manager_v1 *manager, const struct wl_client *client);

#define SECURITY_CONTEXT_MANAGER_V1_VERSION 1

WAYLIB_SERVER_END_NAMESPACE

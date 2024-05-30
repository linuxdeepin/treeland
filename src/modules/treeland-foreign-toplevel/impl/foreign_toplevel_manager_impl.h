// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>

#include <vector>

struct treeland_foreign_toplevel_manager_v1
{
    struct wl_event_loop *event_loop;
    struct wl_global *global;
    struct wl_list resources; // wl_resource_get_link()
    struct wl_list dock_preview;
    struct wl_list toplevels; // treeland_foreign_toplevel_handle_v1.link

    struct wl_listener display_destroy;

    struct
    {
        struct wl_signal dock_preview_created;
        struct wl_signal destroy;
    } events;

    void *data;
};

enum treeland_foreign_toplevel_handle_v1_state {
    TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED = (1 << 0),
    TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED = (1 << 1),
    TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED = (1 << 2),
    TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN = (1 << 3),
};

struct treeland_foreign_toplevel_handle_v1_output
{
    struct wl_list link; // treeland_foreign_toplevel_handle_v1.outputs
    struct wlr_output *output;
    struct treeland_foreign_toplevel_handle_v1 *toplevel;

    // private state

    struct wl_listener output_bind;
    struct wl_listener output_destroy;
};

struct treeland_dock_preview_context_v1
{
    struct treeland_foreign_toplevel_manager_v1 *manager;
    struct wl_resource *resource;
    struct wl_list link;
    struct wl_resource *relative_surface;

    struct
    {
        struct wl_signal request_show;
        struct wl_signal request_close;
        struct wl_signal destroy;
    } events;
};

struct treeland_foreign_toplevel_handle_v1
{
    struct treeland_foreign_toplevel_manager_v1 *manager;
    struct wl_list resources;
    struct wl_list link;
    struct wl_event_source *idle_source;

    char *title;
    char *app_id;
    uint32_t identifier;
    pid_t pid;

    struct treeland_foreign_toplevel_handle_v1 *parent;
    struct wl_list outputs; // treeland_foreign_toplevel_v1_output.link
    uint32_t state;         // enum treeland_foreign_toplevel_v1_state

    struct
    {
        // struct treeland_foreign_toplevel_handle_v1_maximized_event
        struct wl_signal request_maximize;
        // struct treeland_foreign_toplevel_handle_v1_minimized_event
        struct wl_signal request_minimize;
        // struct treeland_foreign_toplevel_handle_v1_activated_event
        struct wl_signal request_activate;
        // struct treeland_foreign_toplevel_handle_v1_fullscreen_event
        struct wl_signal request_fullscreen;
        struct wl_signal request_close;

        // struct treeland_foreign_toplevel_handle_v1_set_rectangle_event
        struct wl_signal set_rectangle;
        struct wl_signal destroy;
    } events;

    void *data;
};

struct treeland_foreign_toplevel_handle_v1_maximized_event
{
    struct treeland_foreign_toplevel_handle_v1 *toplevel;
    bool maximized;
};

struct treeland_foreign_toplevel_handle_v1_minimized_event
{
    struct treeland_foreign_toplevel_handle_v1 *toplevel;
    bool minimized;
};

struct treeland_foreign_toplevel_handle_v1_activated_event
{
    struct treeland_foreign_toplevel_handle_v1 *toplevel;
    struct wlr_seat *seat;
};

struct treeland_foreign_toplevel_handle_v1_fullscreen_event
{
    struct treeland_foreign_toplevel_handle_v1 *toplevel;
    bool fullscreen;
    struct wlr_output *output;
};

struct treeland_foreign_toplevel_handle_v1_set_rectangle_event
{
    struct treeland_foreign_toplevel_handle_v1 *toplevel;
    struct wlr_surface *surface;
    int32_t x, y, width, height;
};

struct treeland_dock_preview_context_v1_preview_event
{
    struct treeland_dock_preview_context_v1 *toplevel;
    std::vector<uint32_t> toplevels;
    int32_t x, y;
    int32_t direction;
};

struct treeland_foreign_toplevel_manager_v1 *treeland_foreign_toplevel_manager_v1_create(
    struct wl_display *display);

struct treeland_foreign_toplevel_handle_v1 *treeland_foreign_toplevel_handle_v1_create(
    struct treeland_foreign_toplevel_manager_v1 *manager);

void treeland_foreign_toplevel_handle_v1_destroy(
    struct treeland_foreign_toplevel_handle_v1 *toplevel);

void treeland_foreign_toplevel_handle_v1_set_title(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, const char *title);
void treeland_foreign_toplevel_handle_v1_set_app_id(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, const char *app_id);
void treeland_foreign_toplevel_handle_v1_set_pid(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, const pid_t pid);
void treeland_foreign_toplevel_handle_v1_set_identifier(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, uint32_t identifier);

void treeland_foreign_toplevel_handle_v1_output_enter(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, struct wlr_output *output);
void treeland_foreign_toplevel_handle_v1_output_leave(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, struct wlr_output *output);

void treeland_foreign_toplevel_handle_v1_set_maximized(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, bool maximized);
void treeland_foreign_toplevel_handle_v1_set_minimized(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, bool minimized);
void treeland_foreign_toplevel_handle_v1_set_activated(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, bool activated);
void treeland_foreign_toplevel_handle_v1_set_fullscreen(
    struct treeland_foreign_toplevel_handle_v1 *toplevel, bool fullscreen);
void treeland_foreign_toplevel_handle_v1_set_parent(
    struct treeland_foreign_toplevel_handle_v1 *toplevel,
    struct treeland_foreign_toplevel_handle_v1 *parent);

void treeland_dock_preview_context_v1_enter(struct treeland_dock_preview_context_v1 *toplevel);
void treeland_dock_preview_context_v1_leave(struct treeland_dock_preview_context_v1 *toplevel);
void treeland_dock_preview_context_v1_destroy(struct treeland_dock_preview_context_v1 *toplevel);

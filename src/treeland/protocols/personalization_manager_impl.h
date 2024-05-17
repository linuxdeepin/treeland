// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>

#include <QStringList>

struct treeland_personalization_manager_v1
{
    struct wl_event_loop *event_loop;
    struct wl_global *global;
    struct wl_list resources; // wl_resource_get_link()

    struct wl_listener display_destroy;

    struct
    {
        struct wl_signal window_context_created;
        struct wl_signal wallpaper_context_created;
        struct wl_signal destroy;
    } events;

    void *data;
};

struct personalization_window_context_v1
{
    struct treeland_personalization_manager_v1 *manager;
    struct wlr_surface *surface;
    struct wl_list link;
    uint32_t background_type;

    struct
    {
        struct wl_signal set_background_type;
        struct wl_signal destroy;
    } events;

    void *data;
};

struct personalization_wallpaper_context_v1
{
    struct treeland_personalization_manager_v1 *manager;
    struct wl_list link;
    struct wl_resource *resource;
    int32_t fd;
    uint32_t uid;
    uint32_t options;
    const char* metaData;
    const char* identifier;

    struct {
        struct wl_signal commit;
        struct wl_signal get_wallpapers;
        struct wl_signal destroy;
    } events;

    void *data;
};

struct treeland_personalization_manager_v1 *treeland_personalization_manager_v1_create(
    struct wl_display *display);

void personalization_wallpaper_v1_send_wallpapers(personalization_wallpaper_context_v1 *wallpaper);
void personalization_window_context_v1_destroy(struct personalization_window_context_v1 *window);
void personalization_wallpaper_context_v1_destroy(
    struct personalization_wallpaper_context_v1 *wallpaper);

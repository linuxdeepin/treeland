// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalization_manager_impl.h"

#include "server-protocol.h"

#include <cassert>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#undef static
}

#define TREELAND_PERSONALIZATION_MANAGEMENT_V1_VERSION 1

struct personalization_window_context_v1 *personalization_window_from_resource(
    struct wl_resource *resource);

static void personalization_window_context_destroy([[maybe_unused]] struct wl_client *client,
                                                   struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void personalization_window_context_set_background_type(
    [[maybe_unused]] struct wl_client *client, struct wl_resource *resource, uint32_t type)
{
    struct personalization_window_context_v1 *window =
        personalization_window_from_resource(resource);
    if (!window) {
        return;
    }

    window->background_type = type;
    wl_signal_emit_mutable(&window->events.set_background_type, window);
}

static const struct personalization_window_context_v1_interface
    personalization_window_context_impl = {
        .set_background_type = personalization_window_context_set_background_type,
        .destroy = personalization_window_context_destroy,
    };

struct personalization_window_context_v1 *personalization_window_from_resource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &personalization_window_context_v1_interface,
                                   &personalization_window_context_impl));
    return static_cast<struct personalization_window_context_v1 *>(
        wl_resource_get_user_data(resource));
}

static void personalization_wallpaper_context_destroy([[maybe_unused]] struct wl_client *client,
                                                      struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

void personalization_window_context_v1_destroy(struct personalization_window_context_v1 *window)
{
    if (!window) {
        return;
    }

    wl_signal_emit_mutable(&window->events.destroy, window);

    wl_list_remove(&window->link);

    free(window);
}

static void personalization_window_context_resource_destroy(struct wl_resource *resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

void set_user_wallpaper(struct wl_client *client,
                        struct wl_resource *manager_resource,
                        int32_t fd,
                        const char *metadata);

void get_user_wallpapers(struct wl_client *client, struct wl_resource *manager_resource);

static const struct personalization_wallpaper_context_v1_interface
    personalization_wallpaper_context_impl = {
        .set_wallpaper = set_user_wallpaper,
        .get_wallpapers = get_user_wallpapers,
        .destroy = personalization_wallpaper_context_destroy,
    };

struct personalization_wallpaper_context_v1 *personalization_wallpaper_from_resource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &personalization_wallpaper_context_v1_interface,
                                   &personalization_wallpaper_context_impl));
    return static_cast<struct personalization_wallpaper_context_v1 *>(
        wl_resource_get_user_data(resource));
}

void personalization_wallpaper_context_v1_destroy(
    struct personalization_wallpaper_context_v1 *wallpaper)
{
    if (!wallpaper) {
        return;
    }

    wl_signal_emit_mutable(&wallpaper->events.destroy, wallpaper);

    wl_list_remove(&wallpaper->link);

    free(wallpaper);
}

static void personalization_wallpaper_context_resource_destroy(struct wl_resource *resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

void create_personalization_window_context_listener(struct wl_client *client,
                                                    struct wl_resource *manager_resource,
                                                    uint32_t id,
                                                    struct wl_resource *surface);

void create_personalization_wallpaper_context_listener(struct wl_client *client,
                                                       struct wl_resource *manager_resource,
                                                       uint32_t id);

static const struct treeland_personalization_manager_v1_interface
    treeland_personalization_manager_impl = {
        .get_window_context = create_personalization_window_context_listener,
        .get_wallpaper_context = create_personalization_wallpaper_context_listener,
    };

struct treeland_personalization_manager_v1 *treeland_personalization_manager_from_resource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_personalization_manager_v1_interface,
                                   &treeland_personalization_manager_impl));
    struct treeland_personalization_manager_v1 *manager =
        static_cast<treeland_personalization_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != NULL);
    return manager;
}

static void treeland_personalization_manager_resource_destroy(struct wl_resource *resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

void create_personalization_window_context_listener(struct wl_client *client,
                                                    struct wl_resource *manager_resource,
                                                    uint32_t id,
                                                    struct wl_resource *surface)
{

    struct treeland_personalization_manager_v1 *manager =
        treeland_personalization_manager_from_resource(manager_resource);
    if (manager == NULL)
        return;

    struct personalization_window_context_v1 *context =
        static_cast<personalization_window_context_v1 *>(calloc(1, sizeof(*context)));

    if (context == NULL) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->manager = manager;

    wl_list_init(&context->link);
    wl_signal_init(&context->events.set_background_type);
    wl_signal_init(&context->events.destroy);

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client, &personalization_window_context_v1_interface, version, id);
    if (resource == NULL) {
        free(context);
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->surface = wlr_surface_from_resource(surface);

    wl_resource_set_implementation(resource,
                                   &personalization_window_context_impl,
                                   context,
                                   personalization_window_context_resource_destroy);

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
    wl_signal_emit_mutable(&manager->events.window_context_created, context);
}

void create_personalization_wallpaper_context_listener(struct wl_client *client,
                                                       struct wl_resource *manager_resource,
                                                       uint32_t id)
{

    struct treeland_personalization_manager_v1 *manager =
        treeland_personalization_manager_from_resource(manager_resource);
    if (manager == NULL)
        return;

    struct personalization_wallpaper_context_v1 *context =
        static_cast<personalization_wallpaper_context_v1 *>(calloc(1, sizeof(*context)));

    if (context == NULL) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->manager = manager;

    wl_list_init(&context->link);
    wl_signal_init(&context->events.set_user_wallpaper);
    wl_signal_init(&context->events.get_user_wallpaper);
    wl_signal_init(&context->events.destroy);

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client, &personalization_wallpaper_context_v1_interface, version, id);
    if (resource == NULL) {
        free(context);
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->resource = resource;
    wl_client_get_credentials(client, nullptr, &context->uid, nullptr);

    wl_resource_set_implementation(resource,
                                   &personalization_wallpaper_context_impl,
                                   context,
                                   personalization_wallpaper_context_resource_destroy);

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
    wl_signal_emit_mutable(&manager->events.wallpaper_context_created, context);
}

void set_user_wallpaper(struct wl_client *client [[maybe_unused]],
                        struct wl_resource *resource,
                        int32_t fd,
                        const char *metadata)
{
    struct personalization_wallpaper_context_v1 *wallpaper =
        personalization_wallpaper_from_resource(resource);
    if (wallpaper == NULL || fd == -1)
        return;

    wallpaper->fd = fd;
    wallpaper->metaData = metadata;
    wl_signal_emit_mutable(&wallpaper->events.set_user_wallpaper, wallpaper);
}

void get_user_wallpapers(struct wl_client *client [[maybe_unused]], struct wl_resource *resource)
{
    struct personalization_wallpaper_context_v1 *wallpaper =
        personalization_wallpaper_from_resource(resource);
    if (wallpaper == NULL)
        return;

    wl_signal_emit_mutable(&wallpaper->events.get_user_wallpaper, wallpaper);
}

void personalization_wallpaper_v1_send_wallpapers(personalization_wallpaper_context_v1 *wallpaper)
{
    if (!wallpaper)
        return;

    personalization_wallpaper_context_v1_send_wallpapers(wallpaper->resource, wallpaper->metaData);
}

static void treeland_personalization_manager_bind(struct wl_client *client,
                                                  void *data,
                                                  uint32_t version,
                                                  uint32_t id)
{
    struct treeland_personalization_manager_v1 *manager =
        static_cast<struct treeland_personalization_manager_v1 *>(data);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_personalization_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource,
                                   &treeland_personalization_manager_impl,
                                   manager,
                                   treeland_personalization_manager_resource_destroy);

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
}

static void handle_display_destroy(struct wl_listener *listener, [[maybe_unused]] void *data)
{
    struct treeland_personalization_manager_v1 *manager =
        wl_container_of(listener, manager, display_destroy);
    wl_signal_emit_mutable(&manager->events.destroy, manager);
    wl_list_remove(&manager->display_destroy.link);
    wl_global_destroy(manager->global);
    free(manager);
}

struct treeland_personalization_manager_v1 *treeland_personalization_manager_v1_create(
    struct wl_display *display)
{
    struct treeland_personalization_manager_v1 *manager =
        static_cast<struct treeland_personalization_manager_v1 *>(calloc(1, sizeof(*manager)));
    if (!manager) {
        return NULL;
    }

    manager->event_loop = wl_display_get_event_loop(display);
    manager->global = wl_global_create(display,
                                       &treeland_personalization_manager_v1_interface,
                                       TREELAND_PERSONALIZATION_MANAGEMENT_V1_VERSION,
                                       manager,
                                       treeland_personalization_manager_bind);
    if (!manager->global) {
        free(manager);
        return NULL;
    }

    wl_signal_init(&manager->events.destroy);
    wl_signal_init(&manager->events.window_context_created);
    wl_signal_init(&manager->events.wallpaper_context_created);
    wl_list_init(&manager->resources);

    manager->display_destroy.notify = handle_display_destroy;
    wl_display_add_destroy_listener(display, &manager->display_destroy);

    return manager;
}

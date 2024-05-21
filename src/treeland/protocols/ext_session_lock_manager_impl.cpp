// Copyright (C) 2024 ssk-wh <fanpengcheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "ext_session_lock_manager_impl.h"

void lock_surface_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void lock_surface_handle_ack_configure([[maybe_unused]] struct wl_client *client,
                                              struct wl_resource *resource,
                                              uint32_t serial)
{
    struct ext_session_lock_surface_v1 *context =
        static_cast<ext_session_lock_surface_v1 *>(wl_resource_get_user_data(resource));
    if (!context) {
        return;
    }

    wl_signal_emit_mutable(&context->events.ack_configure, context);
}

static const struct ext_session_lock_surface_v1_interface lock_surface_implementation = {
    .destroy = lock_surface_handle_destroy,
    .ack_configure = lock_surface_handle_ack_configure,
};

void ext_session_lock_surface_v1_destroy(struct ext_session_lock_surface_v1 *context)
{
    wl_signal_emit_mutable(&context->events.destroy, context);
    free(context);
}

void ext_session_lock_surface_v1_destroy_func(wl_resource *resource)
{
    struct ext_session_lock_surface_v1 *context =
        static_cast<ext_session_lock_surface_v1 *>(wl_resource_get_user_data(resource));
    if (!context) {
        return;
    }

    ext_session_lock_surface_v1_destroy(context);
}

static void lock_handle_get_lock_surface(struct wl_client *client,
                                         struct wl_resource *lock_resource,
                                         uint32_t id,
                                         struct wl_resource *surface,
                                         struct wl_resource *output)
{
    struct ext_session_lock_v1 *context =
        static_cast<ext_session_lock_v1 *>(wl_resource_get_user_data(lock_resource));
    struct wl_resource *resource = wl_resource_create(client,
                                                      &ext_session_lock_surface_v1_interface,
                                                      EXT_SESSION_LOCK_V1_DESTROY_SINCE_VERSION,
                                                      id);
    if (resource == NULL) {
        wl_resource_post_no_memory(lock_resource);
        return;
    }

    struct ext_session_lock_surface_v1 *lock_surface =
        static_cast<ext_session_lock_surface_v1 *>(calloc(1, sizeof(*lock_surface)));
    if (lock_surface == NULL) {
        wl_resource_post_no_memory(lock_resource);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &lock_surface_implementation,
                                   lock_surface,
                                   ext_session_lock_surface_v1_destroy_func);

    wl_resource_set_user_data(resource, lock_surface);

    wl_signal_init(&lock_surface->events.ack_configure);
    wl_signal_init(&lock_surface->events.destroy);

    lock_surface->resource = resource;
    lock_surface->surface = surface;
    lock_surface->output = output;
    lock_surface->id = id;

    wl_signal_emit_mutable(&context->events.get_lock_surface, lock_surface);
}

static void lock_handle_destroy([[maybe_unused]] struct wl_client *client,
                                struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void lock_handle_unlock_and_destroy(struct wl_client *client, struct wl_resource *resource)
{
    struct ext_session_lock_v1 *context =
        static_cast<ext_session_lock_v1 *>(wl_resource_get_user_data(resource));
    wl_signal_emit_mutable(&context->events.unlock_and_destroy, context);
    lock_handle_destroy(client, resource);
}

static const struct ext_session_lock_v1_interface lock_implementation = {
    .destroy = lock_handle_destroy,
    .get_lock_surface = lock_handle_get_lock_surface,
    .unlock_and_destroy = lock_handle_unlock_and_destroy,
};

static void manager_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
    struct ext_session_lock_manager_v1 *context =
        static_cast<ext_session_lock_manager_v1 *>(wl_resource_get_user_data(resource));
    if (!context) {
        return;
    }

    wl_signal_emit_mutable(&context->events.destroy, context);
    wl_list_remove(wl_resource_get_link(resource));
}

void ext_session_lock_v1_destroy(struct ext_session_lock_v1 *context)
{
    wl_signal_emit_mutable(&context->events.destroy, context);
    free(context);
}

void ext_session_lock_v1_destroy_func(wl_resource *resource)
{
    struct ext_session_lock_v1 *context =
        static_cast<ext_session_lock_v1 *>(wl_resource_get_user_data(resource));
    if (!context) {
        return;
    }

    ext_session_lock_v1_destroy(context);
}

static void manager_handle_lock(struct wl_client *client,
                                struct wl_resource *manager_resource,
                                uint32_t id)
{
    struct ext_session_lock_manager_v1 *manager =
        static_cast<ext_session_lock_manager_v1 *>(wl_resource_get_user_data(manager_resource));

    struct wl_resource *resource = wl_resource_create(client,
                                                      &ext_session_lock_v1_interface,
                                                      EXT_SESSION_LOCK_V1_DESTROY_SINCE_VERSION,
                                                      id);
    if (resource == NULL) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    struct ext_session_lock_v1 *context =
        static_cast<ext_session_lock_v1 *>(calloc(1, sizeof(*context)));
    if (context == NULL) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &lock_implementation,
                                   context,
                                   ext_session_lock_v1_destroy_func);
    wl_resource_set_user_data(resource, context);

    wl_signal_init(&context->events.get_lock_surface);
    wl_signal_init(&context->events.unlock_and_destroy);
    wl_signal_init(&context->events.destroy);

    context->resource = resource;
    context->id = id;
    wl_list_init(&context->contexts);
    wl_list_insert(&manager->contexts, wl_resource_get_link(resource));

    wl_signal_emit_mutable(&manager->events.lock, context);
}

static const struct ext_session_lock_manager_v1_interface lock_manager_implementation = {
    .destroy = manager_handle_destroy,
    .lock = manager_handle_lock,
};

static void bind_ext_session_lock_manager_v1(struct wl_client *client,
                                             void *data,
                                             uint32_t version,
                                             uint32_t id)
{
    struct ext_session_lock_manager_v1 *manager =
        static_cast<struct ext_session_lock_manager_v1 *>(data);
    struct wl_resource *resource =
        wl_resource_create(client, &ext_session_lock_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &lock_manager_implementation, manager, NULL);

    wl_list_insert(&manager->contexts, wl_resource_get_link(resource));

    manager->client = resource;
}

static void handle_display_destroy(struct wl_listener *listener, [[maybe_unused]] void *data)
{
    struct ext_session_lock_manager_v1 *manager =
        wl_container_of(listener, manager, display_destroy);
    wl_signal_emit_mutable(&manager->events.destroy, manager);
    wl_list_remove(&manager->display_destroy.link);
    wl_global_destroy(manager->global);
    free(manager);
}

#define SESSION_LOCK_MANAGEMENT_V1_VERSION 1

ext_session_lock_manager_v1 *ext_session_lock_manager_v1_create(wl_display *display)
{
    struct ext_session_lock_manager_v1 *manager =
        static_cast<struct ext_session_lock_manager_v1 *>(calloc(1, sizeof(*manager)));
    if (!manager) {
        return NULL;
    }

    manager->event_loop = wl_display_get_event_loop(display);
    manager->global = wl_global_create(display,
                                       &ext_session_lock_manager_v1_interface,
                                       SESSION_LOCK_MANAGEMENT_V1_VERSION,
                                       manager,
                                       bind_ext_session_lock_manager_v1);
    if (!manager->global) {
        free(manager);
        return NULL;
    }

    wl_signal_init(&manager->events.lock);
    wl_signal_init(&manager->events.destroy);
    wl_list_init(&manager->contexts);

    manager->display_destroy.notify = handle_display_destroy;
    wl_display_add_destroy_listener(display, &manager->display_destroy);

    return manager;
}

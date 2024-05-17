// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcut_manager_impl.h"

#include "shortcut-server-protocol.h"

#include <QMetaObject>
#include <QObject>

#include <map>

#define SHORTCUT_MANAGEMENT_V1_VERSION 1

static std::map<struct wl_resource *, QMetaObject::Connection> CLIENT_CONNECT;

void treeland_shortcut_context_v1_destroy(struct wl_resource *resource);

static void treeland_shortcut_context_destroy([[maybe_unused]] struct wl_client *client,
                                              struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static const struct treeland_shortcut_context_v1_interface shortcut_context_impl
{
    .destroy = treeland_shortcut_context_destroy,
};

struct treeland_shortcut_manager_v1 *shortcut_manager_from_resource(struct wl_resource *resource);

void shortcut_manager_resource_destroy(struct wl_resource *resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

void treeland_shortcut_context_v1_destroy(struct wl_resource *resource)
{
    struct treeland_shortcut_context_v1 *context =
        static_cast<treeland_shortcut_context_v1 *>(wl_resource_get_user_data(resource));
    if (!context) {
        return;
    }

    treeland_shortcut_context_v1_destroy(context);
}

void treeland_shortcut_context_v1_destroy(struct treeland_shortcut_context_v1 *context)
{
    wl_signal_emit_mutable(&context->events.destroy, context);

    wl_list_remove(&context->link);

    free(context);
}

void treeland_shortcut_context_v1_send_shortcut(struct treeland_shortcut_context_v1 *context)
{
    treeland_shortcut_context_v1_send_shortcut(context->resource);
}

void treeland_shortcut_context_v1_send_register_failed(struct treeland_shortcut_context_v1 *context)
{
    wl_resource_post_error(context->resource,
                           TREELAND_SHORTCUT_CONTEXT_V1_ERROR_REGISTER_FAILED,
                           "register shortcut failed.");
}

void create_shortcut_context_listener(struct wl_client *client,
                                      struct wl_resource *manager_resource,
                                      const char *key,
                                      uint32_t id)
{
    struct treeland_shortcut_manager_v1 *manager = shortcut_manager_from_resource(manager_resource);

    struct wl_resource *resource =
        wl_resource_create(client,
                           &treeland_shortcut_context_v1_interface,
                           TREELAND_SHORTCUT_CONTEXT_V1_SHORTCUT_SINCE_VERSION,
                           id);
    if (resource == NULL) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    struct treeland_shortcut_context_v1 *context =
        static_cast<treeland_shortcut_context_v1 *>(calloc(1, sizeof(*context)));
    if (context == NULL) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &shortcut_context_impl,
                                   context,
                                   treeland_shortcut_context_v1_destroy);

    wl_resource_set_user_data(resource, context);

    wl_signal_init(&context->events.destroy);

    context->manager = manager;
    context->key = strdup(key);
    context->resource = resource;

    wl_list_insert(&manager->contexts, &context->link);

    wl_signal_emit_mutable(&manager->events.context, context);
}

static const struct treeland_shortcut_manager_v1_interface shortcut_manager_impl
{
    .register_shortcut_context = create_shortcut_context_listener,
};

struct treeland_shortcut_manager_v1 *shortcut_manager_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_shortcut_manager_v1_interface,
                                   &shortcut_manager_impl));
    struct treeland_shortcut_manager_v1 *manager =
        static_cast<treeland_shortcut_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != NULL);
    return manager;
}

static void treeland_shortcut_manager_bind(struct wl_client *client,
                                           void *data,
                                           uint32_t version,
                                           uint32_t id)
{
    struct treeland_shortcut_manager_v1 *manager =
        static_cast<struct treeland_shortcut_manager_v1 *>(data);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_shortcut_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource,
                                   &shortcut_manager_impl,
                                   manager,
                                   shortcut_manager_resource_destroy);

    wl_list_insert(&manager->contexts, wl_resource_get_link(resource));

    manager->client = resource;
}

static void handle_display_destroy(struct wl_listener *listener, [[maybe_unused]] void *data)
{
    struct treeland_shortcut_manager_v1 *manager =
        wl_container_of(listener, manager, display_destroy);
    wl_signal_emit_mutable(&manager->events.destroy, manager);
    wl_list_remove(&manager->display_destroy.link);
    wl_global_destroy(manager->global);
    free(manager);
}

struct treeland_shortcut_manager_v1 *treeland_shortcut_manager_v1_create(struct wl_display *display)
{
    struct treeland_shortcut_manager_v1 *manager =
        static_cast<struct treeland_shortcut_manager_v1 *>(calloc(1, sizeof(*manager)));
    if (!manager) {
        return NULL;
    }

    manager->event_loop = wl_display_get_event_loop(display);
    manager->global = wl_global_create(display,
                                       &treeland_shortcut_manager_v1_interface,
                                       SHORTCUT_MANAGEMENT_V1_VERSION,
                                       manager,
                                       treeland_shortcut_manager_bind);
    if (!manager->global) {
        free(manager);
        return NULL;
    }

    wl_signal_init(&manager->events.context);
    wl_signal_init(&manager->events.destroy);
    wl_list_init(&manager->contexts);

    manager->display_destroy.notify = handle_display_destroy;
    wl_display_add_destroy_listener(display, &manager->display_destroy);

    return manager;
}

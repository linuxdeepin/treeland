// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreign_toplevel_manager_impl.h"

#include "foreign-toplevel-manager-server-protocol.h"
#include "utils.h"

#include <QDebug>

#include <cassert>

static const struct ztreeland_foreign_toplevel_handle_v1_interface toplevel_handle_impl
{
    .destroy = resource_handle_destroy,
};

void foreign_toplevel_manager_handle_stop(struct wl_client *client, struct wl_resource *resource);

static const struct ztreeland_foreign_toplevel_manager_v1_interface foreign_toplevel_manager_impl
{
    .stop = foreign_toplevel_manager_handle_stop
};

void foreign_toplevel_manager_handle_stop([[maybe_unused]] struct wl_client *client,
                                          [[maybe_unused]] struct wl_resource *resource)
{
    qDebug() << Q_FUNC_INFO;
}

struct ztreeland_foreign_toplevel_handle_v1 *ztreeland_foreign_toplevel_handle_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &ztreeland_foreign_toplevel_handle_v1_interface,
                                   &toplevel_handle_impl));
    return static_cast<ztreeland_foreign_toplevel_handle_v1 *>(wl_resource_get_user_data(resource));
}

void ztreeland_foreign_toplevel_handle_resource_destroy(struct wl_resource *resource)
{
    struct ztreeland_foreign_toplevel_handle_v1 *context = ztreeland_foreign_toplevel_handle_from_resource(resource);

    context_destroy(context);
}

struct ztreeland_foreign_toplevel_manager_v1 *
foreign_toplevel_manager_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &ztreeland_foreign_toplevel_manager_v1_interface,
                                   &foreign_toplevel_manager_impl));
    struct ztreeland_foreign_toplevel_manager_v1 *manager =
        static_cast<ztreeland_foreign_toplevel_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != NULL);
    return manager;
}

struct ztreeland_foreign_toplevel_handle_v1 *
create_toplevel_handle(struct ztreeland_foreign_toplevel_manager_v1 *manager,
                       struct wl_resource *client,
                       struct wl_resource *surface)
{
    struct ztreeland_foreign_toplevel_handle_v1 *handle = new ztreeland_foreign_toplevel_handle_v1;
    struct wl_resource *handle_resource =
        wl_resource_create(wl_resource_get_client(client),
                           &ztreeland_foreign_toplevel_handle_v1_interface,
                           wl_resource_get_version(client),
                           0);
    if (!handle_resource) {
        wl_client_post_no_memory(wl_resource_get_client(client));
        return {};
    }

    wl_resource_set_implementation(handle_resource,
                                   &toplevel_handle_impl,
                                   handle,
                                   ztreeland_foreign_toplevel_handle_resource_destroy);

    wl_list_insert(&manager->contexts, &handle->link);

    handle->manager = manager;
    handle->resource = handle_resource;
    handle->surface = surface;

    return handle;
}

void ztreeland_foreign_toplevel_list_v1_destroy(struct wl_resource *resource)
{
    struct ztreeland_foreign_toplevel_manager_v1 *handle = foreign_toplevel_manager_from_resource(resource);
    std::erase_if(handle->clients, [=](auto *c) {
        return c == resource;
    });
}

void foreign_toplevel_manager_bind(struct wl_client *client,
                                   void *data,
                                   uint32_t version,
                                   uint32_t id)
{
    struct ztreeland_foreign_toplevel_manager_v1 *manager =
        static_cast<ztreeland_foreign_toplevel_manager_v1 *>(data);

    struct wl_resource *resource =
        wl_resource_create(client, &ztreeland_foreign_toplevel_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &foreign_toplevel_manager_impl, manager, ztreeland_foreign_toplevel_list_v1_destroy);

    manager->clients.push_back(resource);

    // send all surface to client.
    for (auto *surface : manager->surfaces) {
        auto *toplevel = create_toplevel_handle(manager, resource, surface);
        ztreeland_foreign_toplevel_manager_v1_send_toplevel(resource, toplevel->resource);
        wl_signal_emit_mutable(&manager->events.handleCreated, surface);
    }
}

void foreign_toplevel_manager_handle_display_destroy(struct wl_listener *listener, [[maybe_unused]] void *data)
{
    struct ztreeland_foreign_toplevel_manager_v1 *manager =
        wl_container_of(listener, manager, display_destroy);
    wl_signal_emit_mutable(&manager->events.destroy, manager);
    assert(wl_list_empty(&manager->events.destroy.listener_list));

    struct ztreeland_foreign_toplevel_handle_v1 *context;
    struct ztreeland_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &manager->contexts, link)
    {
        context_destroy(context);
    }

    wl_global_destroy(manager->global);
    wl_list_remove(&manager->display_destroy.link);
    free(manager);
}

struct ztreeland_foreign_toplevel_manager_v1 *
ztreeland_foreign_toplevel_manager_v1_create(struct wl_display *display)
{
    struct ztreeland_foreign_toplevel_manager_v1 *handle =
        new ztreeland_foreign_toplevel_manager_v1;

    handle->event_loop = wl_display_get_event_loop(display);
    handle->global = wl_global_create(display,
                                      &ztreeland_foreign_toplevel_manager_v1_interface,
                                      ZTREELAND_FOREIGN_TOPLEVEL_MANAGER_V1_TOPLEVEL_SINCE_VERSION,
                                      handle,
                                      foreign_toplevel_manager_bind);
    if (!handle->global) {
        delete handle;
        return nullptr;
    }

    wl_signal_init(&handle->events.destroy);
    wl_signal_init(&handle->events.handleCreated);
    wl_list_init(&handle->contexts);

    handle->display_destroy.notify = foreign_toplevel_manager_handle_display_destroy;
    wl_display_add_destroy_listener(display, &handle->display_destroy);
    return handle;
}

void ztreeland_foreign_toplevel_manager_v1_destroy(
    struct ztreeland_foreign_toplevel_manager_v1 *handle)
{
    wl_signal_emit_mutable(&handle->events.destroy, handle);
    assert(wl_list_empty(&handle->events.destroy.listener_list));

    struct ztreeland_foreign_toplevel_handle_v1 *context;
    struct ztreeland_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        context_destroy(context);
    }

    wl_global_destroy(handle->global);
    wl_list_remove(&handle->display_destroy.link);
    free(handle);
}

void ztreeland_foreign_toplevel_manager_v1_toplevel(
    struct ztreeland_foreign_toplevel_manager_v1 *handle, struct wl_resource *resource)
{
    handle->surfaces.push_back(resource);

    // send surface to all clients.
    for (auto *client : handle->clients) {
        auto *toplevel = create_toplevel_handle(handle, client, resource);
        ztreeland_foreign_toplevel_manager_v1_send_toplevel(client, toplevel->resource);
        wl_signal_emit_mutable(&handle->events.handleCreated, resource);
    }
}

void ztreeland_foreign_toplevel_handle_v1_closed(
    struct ztreeland_foreign_toplevel_manager_v1 *handle, struct wl_resource *resource)
{
    struct ztreeland_foreign_toplevel_handle_v1 *context;
    struct ztreeland_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        if (context->surface == resource) {
            ztreeland_foreign_toplevel_handle_v1_send_closed(context->resource);
            context->surface = nullptr;
        }
    }

    std::erase_if(handle->surfaces, [=](auto *h) {
        return h == resource;
    });
}

void ztreeland_foreign_toplevel_handle_v1_done(struct ztreeland_foreign_toplevel_manager_v1 *handle,
                                               struct wl_resource *resource)
{
    struct ztreeland_foreign_toplevel_handle_v1 *context;
    struct ztreeland_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        if (context->surface == resource) {
            ztreeland_foreign_toplevel_handle_v1_send_done(context->resource);
        }
    }
}

void ztreeland_foreign_toplevel_handle_v1_pid(struct ztreeland_foreign_toplevel_manager_v1 *handle,
                                              struct wl_resource *resource,
                                              uint32_t pid)
{
    struct ztreeland_foreign_toplevel_handle_v1 *context;
    struct ztreeland_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        if (context->surface == resource) {
            ztreeland_foreign_toplevel_handle_v1_send_pid(context->resource, pid);
        }
    }
}

void ztreeland_foreign_toplevel_handle_v1_identifier(
    struct ztreeland_foreign_toplevel_manager_v1 *handle,
    struct wl_resource *resource,
    const QString &identifier)
{
    struct ztreeland_foreign_toplevel_handle_v1 *context;
    struct ztreeland_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        if (context->surface == resource) {
            ztreeland_foreign_toplevel_handle_v1_send_identifier(context->resource,
                                                                 identifier.toUtf8());
        }
    }
}

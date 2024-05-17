// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ext_foreign_toplevel_list_impl.h"

#include "ext-foreign-toplevel-list-server-protocol.h"
#include "utils.h"

#include <pixman.h>

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <qwcompositor.h>

#include <QDebug>

#include <vector>

void ext_foreign_toplevel_handle_destroy(struct wl_client *, struct wl_resource *resource);
void ext_foreign_toplevel_handle_resource_destroy(struct wl_resource *resource);

static const struct ext_foreign_toplevel_handle_v1_interface toplevel_handle_impl
{
    .destroy = ext_foreign_toplevel_handle_destroy,
};

void foreign_toplevel_list_handle_stop(struct wl_client *client, struct wl_resource *resource);

struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_from_resource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &ext_foreign_toplevel_handle_v1_interface,
                                   &toplevel_handle_impl));
    return static_cast<ext_foreign_toplevel_handle_v1 *>(wl_resource_get_user_data(resource));
}

void ext_foreign_toplevel_handle_destroy(struct wl_client *, struct wl_resource *resource)
{
    ext_foreign_toplevel_handle_resource_destroy(resource);
}

void ext_foreign_toplevel_handle_resource_destroy(struct wl_resource *resource)
{
    struct ext_foreign_toplevel_handle_v1 *context =
        ext_foreign_toplevel_handle_from_resource(resource);

    context_destroy(context);
}

static const struct ext_foreign_toplevel_list_v1_interface toplevel_list_impl
{
    .stop = foreign_toplevel_list_handle_stop, .destroy = resource_handle_destroy
};

void foreign_toplevel_list_handle_stop([[maybe_unused]] struct wl_client *client,
                                       struct wl_resource *resource)
{
    [[maybe_unused]] struct ext_foreign_toplevel_list_v1 *handle =
        foreign_toplevel_list_from_resource(resource);

    // TODO: stop
}

struct ext_foreign_toplevel_list_v1 *foreign_toplevel_list_from_resource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &ext_foreign_toplevel_list_v1_interface,
                                   &toplevel_list_impl));
    struct ext_foreign_toplevel_list_v1 *list =
        static_cast<ext_foreign_toplevel_list_v1 *>(wl_resource_get_user_data(resource));
    assert(list != NULL);
    return list;
}

struct ext_foreign_toplevel_handle_v1 *create_toplevel_handle(
    struct ext_foreign_toplevel_list_v1 *manager,
    struct wl_resource *client,
    struct wl_resource *surface)
{
    struct ext_foreign_toplevel_handle_v1 *handle = new ext_foreign_toplevel_handle_v1;
    struct wl_resource *handle_resource =
        wl_resource_create(wl_resource_get_client(client),
                           &ext_foreign_toplevel_handle_v1_interface,
                           wl_resource_get_version(client),
                           0);
    if (!handle_resource) {
        wl_client_post_no_memory(wl_resource_get_client(client));
        return {};
    }

    wl_resource_set_implementation(handle_resource,
                                   &toplevel_handle_impl,
                                   handle,
                                   ext_foreign_toplevel_handle_resource_destroy);

    wl_list_insert(&manager->contexts, &handle->link);

    handle->manager = manager;
    handle->resource = handle_resource;
    handle->surface = surface;

    return handle;
}

void ext_foreign_toplevel_list_v1_destroy(struct wl_resource *resource)
{
    struct ext_foreign_toplevel_list_v1 *handle = foreign_toplevel_list_from_resource(resource);
    std::erase_if(handle->clients, [=](auto *c) {
        return c == resource;
    });
}

void ext_foreign_toplevel_list_bind(struct wl_client *client,
                                    void *data,
                                    uint32_t version,
                                    uint32_t id)
{
    struct ext_foreign_toplevel_list_v1 *handle = static_cast<ext_foreign_toplevel_list_v1 *>(data);

    struct wl_resource *resource =
        wl_resource_create(client, &ext_foreign_toplevel_list_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource,
                                   &toplevel_list_impl,
                                   handle,
                                   ext_foreign_toplevel_list_v1_destroy);

    handle->clients.push_back(resource);

    // send all surface to client.
    for (auto *surface : handle->surfaces) {
        auto *toplevel = create_toplevel_handle(handle, resource, surface);
        ext_foreign_toplevel_list_v1_send_toplevel(resource, toplevel->resource);
        wl_signal_emit_mutable(&handle->events.handleCreated, surface);
    }
}

void ext_foreign_toplevel_list_handle_display_destroy(struct wl_listener *listener,
                                                      [[maybe_unused]] void *data)
{
    struct ext_foreign_toplevel_list_v1 *list = wl_container_of(listener, list, display_destroy);
    ext_foreign_toplevel_list_v1_destroy(list);
}

struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1_create(struct wl_display *display)
{
    struct ext_foreign_toplevel_list_v1 *handle = new ext_foreign_toplevel_list_v1;

    handle->event_loop = wl_display_get_event_loop(display);
    handle->global = wl_global_create(display,
                                      &ext_foreign_toplevel_list_v1_interface,
                                      EXT_FOREIGN_TOPLEVEL_LIST_V1_TOPLEVEL_SINCE_VERSION,
                                      handle,
                                      ext_foreign_toplevel_list_bind);
    if (!handle->global) {
        delete handle;
        return nullptr;
    }

    wl_signal_init(&handle->events.destroy);
    wl_signal_init(&handle->events.handleCreated);
    wl_list_init(&handle->contexts);

    handle->display_destroy.notify = ext_foreign_toplevel_list_handle_display_destroy;
    wl_display_add_destroy_listener(display, &handle->display_destroy);
    return handle;
}

void ext_foreign_toplevel_list_v1_destroy(struct ext_foreign_toplevel_list_v1 *handle)
{
    wl_signal_emit_mutable(&handle->events.destroy, handle);
    assert(wl_list_empty(&handle->events.destroy.listener_list));

    struct ext_foreign_toplevel_handle_v1 *context;
    struct ext_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        context_destroy(context);
    }

    wl_global_destroy(handle->global);
    wl_list_remove(&handle->display_destroy.link);
    free(handle);
}

void ext_foreign_toplevel_list_v1_toplevel(struct ext_foreign_toplevel_list_v1 *handle,
                                           struct wl_resource *resource)
{
    handle->surfaces.push_back(resource);

    // send surface to all clients.
    for (auto *client : handle->clients) {
        auto *toplevel = create_toplevel_handle(handle, client, resource);
        ext_foreign_toplevel_list_v1_send_toplevel(client, toplevel->resource);
        wl_signal_emit_mutable(&handle->events.handleCreated, resource);
    }
}

void ext_foreign_toplevel_handle_v1_closed(struct ext_foreign_toplevel_list_v1 *handle,
                                           struct wl_resource *resource)
{
    struct ext_foreign_toplevel_handle_v1 *context;
    struct ext_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        if (context->surface == resource) {
            ext_foreign_toplevel_handle_v1_send_closed(context->resource);
            context->surface = nullptr;
        }
    }

    std::erase_if(handle->surfaces, [=](auto *h) {
        return h == resource;
    });
}

void ext_foreign_toplevel_handle_v1_done(struct ext_foreign_toplevel_list_v1 *handle,
                                         struct wl_resource *resource)
{
    struct ext_foreign_toplevel_handle_v1 *context;
    struct ext_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        if (context->surface == resource) {
            ext_foreign_toplevel_handle_v1_send_done(context->resource);
        }
    }
}

void ext_foreign_toplevel_handle_v1_title(struct ext_foreign_toplevel_list_v1 *handle,
                                          struct wl_resource *resource,
                                          const QString &title)
{
    struct ext_foreign_toplevel_handle_v1 *context;
    struct ext_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        if (context->surface == resource) {
            ext_foreign_toplevel_handle_v1_send_title(context->resource, title.toUtf8());
        }
    }
}

void ext_foreign_toplevel_handle_v1_app_id(struct ext_foreign_toplevel_list_v1 *handle,
                                           struct wl_resource *resource,
                                           const QString &appId)
{
    struct ext_foreign_toplevel_handle_v1 *context;
    struct ext_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        if (context->surface == resource) {
            ext_foreign_toplevel_handle_v1_send_app_id(context->resource, appId.toUtf8());
        }
    }
}

void ext_foreign_toplevel_handle_v1_identifier(struct ext_foreign_toplevel_list_v1 *handle,
                                               struct wl_resource *resource,
                                               const QString &identifier)
{
    struct ext_foreign_toplevel_handle_v1 *context;
    struct ext_foreign_toplevel_handle_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &handle->contexts, link)
    {
        if (context->surface == resource) {
            ext_foreign_toplevel_handle_v1_send_identifier(context->resource, identifier.toUtf8());
        }
    }
}

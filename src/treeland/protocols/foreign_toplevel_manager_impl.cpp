// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreign_toplevel_manager_impl.h"
#include "foreign-toplevel-manager-server-protocol.h"
#include "utils.h"

#include <cassert>
#include <QDebug>

static const struct ztreeland_foreign_toplevel_handle_v1_interface toplevel_handle_impl{
    .destroy = resource_handle_destroy,
};

void foreign_toplevel_manager_handle_stop(struct wl_client *client, struct wl_resource *resource);

static const struct ztreeland_foreign_toplevel_manager_v1_interface foreign_toplevel_manager_impl {
    .stop = foreign_toplevel_manager_handle_stop
};

void foreign_toplevel_manager_handle_stop(struct wl_client *client, struct wl_resource *resource)
{
    qDebug() << Q_FUNC_INFO;
}

struct ztreeland_foreign_toplevel_manager_v1 *foreign_toplevel_manager_from_resource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(
        resource, &ztreeland_foreign_toplevel_manager_v1_interface, &foreign_toplevel_manager_impl));
    struct ztreeland_foreign_toplevel_manager_v1 *manager =
        static_cast<ztreeland_foreign_toplevel_manager_v1 *>(
            wl_resource_get_user_data(resource));
    assert(manager != NULL);
    return manager;
}

void foreign_toplevel_manager_bind(struct wl_client *client,
                         void             *data,
                         uint32_t          version,
                         uint32_t          id)
{
    struct ztreeland_foreign_toplevel_manager_v1 *manager =
        static_cast<ztreeland_foreign_toplevel_manager_v1 *>(data);

    struct wl_resource *resource = wl_resource_create(
        client, &ztreeland_foreign_toplevel_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &foreign_toplevel_manager_impl, manager,
                                   NULL);
}

void foreign_toplevel_manager_handle_display_destroy(struct wl_listener *listener, void *data)
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

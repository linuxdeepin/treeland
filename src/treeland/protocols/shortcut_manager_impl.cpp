// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcut_manager_impl.h"

#include <QObject>
#include <map>
#include <QMetaObject>

#include "utils.h"
#include "shortcut-server-protocol.h"
#include "shortcutmanager.h"

static std::map<struct wl_resource*, QMetaObject::Connection> CLIENT_CONNECT;

static const struct ztreeland_shortcut_context_v1_interface shortcut_context_impl{
    .destroy = resource_handle_destroy,
};

struct ztreeland_shortcut_manager_v1 *shortcut_manager_from_resource(struct wl_resource *resource);

struct ztreeland_shortcut_context_v1 *shortcut_context_from_resource(struct wl_resource *resource) {
    assert(wl_resource_instance_of(resource, &ztreeland_shortcut_context_v1_interface, &shortcut_context_impl));
    return static_cast<ztreeland_shortcut_context_v1*>(wl_resource_get_user_data(resource));
}

void shortcut_context_resource_destroy(struct wl_resource *resource) {
    struct ztreeland_shortcut_context_v1 *context = shortcut_context_from_resource(resource);

    context_destroy(context);
}

void create_shortcut_context_listener(struct wl_client *client,
                                           struct wl_resource *manager_resource,
                                           uint32_t id)
{

    struct ztreeland_shortcut_manager_v1 *manager = shortcut_manager_from_resource(manager_resource);

    struct ztreeland_shortcut_context_v1 *context = static_cast<ztreeland_shortcut_context_v1*>(calloc(1, sizeof(*context)));
    if (context == NULL) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->manager = manager;

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource = wl_resource_create(client, &ztreeland_shortcut_context_v1_interface, version, id);
    if (resource == NULL) {
        free(context);
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    wl_resource_set_implementation(resource, &shortcut_context_impl, context, shortcut_context_resource_destroy);

    wl_list_insert(&manager->contexts, &context->link);

    QMetaObject::Connection connect = QObject::connect(context->manager->manager->helper(), &TreeLandHelper::keyEvent, qApp, [resource](int key, int modify) {
        ztreeland_shortcut_context_v1_send_shortcut(resource, static_cast<uint32_t>(key), static_cast<uint32_t>(modify));
    });

    CLIENT_CONNECT[resource] = connect;
}

static const struct ztreeland_shortcut_manager_v1_interface shortcut_manager_impl {
    .get_shortcut_context = create_shortcut_context_listener,
};

struct ztreeland_shortcut_manager_v1 *shortcut_manager_from_resource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(
        resource, &ztreeland_shortcut_manager_v1_interface, &shortcut_manager_impl));
    struct ztreeland_shortcut_manager_v1 *manager =
        static_cast<ztreeland_shortcut_manager_v1 *>(
            wl_resource_get_user_data(resource));
    assert(manager != NULL);
    return manager;
}

void shortcut_manager_bind(struct wl_client *client,
                         void             *data,
                         uint32_t          version,
                         uint32_t          id)
{
    struct ztreeland_shortcut_manager_v1 *manager =
        static_cast<ztreeland_shortcut_manager_v1 *>(data);

    struct wl_resource *resource = wl_resource_create(
        client, &ztreeland_shortcut_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &shortcut_manager_impl, manager,
                                   NULL);
}

void shortcut_manager_handle_display_destroy(struct wl_listener *listener, [[maybe_unused]] void *data)
{
    struct ztreeland_shortcut_manager_v1 *manager =
        wl_container_of(listener, manager, display_destroy);
    wl_signal_emit_mutable(&manager->events.destroy, manager);
    assert(wl_list_empty(&manager->events.destroy.listener_list));

    struct ztreeland_shortcut_context_v1 *context;
    struct ztreeland_shortcut_context_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &manager->contexts, link)
    {
        context_destroy(context);
    }

    wl_global_destroy(manager->global);
    wl_list_remove(&manager->display_destroy.link);
    free(manager);
}

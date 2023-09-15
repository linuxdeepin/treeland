// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcut.h"
#include "shortcut-server-protocol.h"
#include "treeland.h"
#include "helper.h"

#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <QDebug>
#include <wayland-server-core.h>

std::map<struct wl_resource*, QMetaObject::Connection> CLIENT_CONNECT;

struct treeland_shortcut_context_v1 {
    struct treeland_shortcut_manager_v1 *manager;
    struct treeland_shortcut_context_v1_state state;
    struct wl_list link; // treeland_shortcut_manager_v1.contexts
};

static struct treeland_shortcut_context_v1 *context_from_resource(struct wl_resource *resource);
static struct treeland_shortcut_manager_v1 *manager_from_resource(struct wl_resource *resource);

static void resource_handle_destroy(struct wl_client *, struct wl_resource *resource) {
    wl_resource_destroy(resource);
}

static void context_handle_listen(struct wl_client *, struct wl_resource *resource) {
    struct treeland_shortcut_context_v1 *context = context_from_resource(resource);
    if (!context) {
        return;
    }

    wl_resource_set_user_data(resource, NULL);

    wl_list_insert(&context->manager->contexts, &context->link);

    QMetaObject::Connection connect = QObject::connect(context->manager->helper, &Helper::keyEvent, qApp, [resource](int key, int modify) {
        treeland_shortcut_context_v1_send_shortcut(resource, key, modify);
    });

    CLIENT_CONNECT[resource] = connect;
}

static const struct treeland_shortcut_context_v1_interface shortcut_context_impl{
    .listen = context_handle_listen,
    .destroy = resource_handle_destroy,
};

static struct treeland_shortcut_context_v1 *context_from_resource(struct wl_resource *resource) {
    assert(wl_resource_instance_of(resource, &treeland_shortcut_context_v1_interface, &shortcut_context_impl));
    return static_cast<treeland_shortcut_context_v1*>(wl_resource_get_user_data(resource));
}

static void context_destroy(struct treeland_shortcut_context_v1 *context) {
    if (context == NULL) {
        return;
    }

    wl_list_remove(&context->link);
    free(context);
}

static void context_resource_destroy(struct wl_resource *resource) {
    struct treeland_shortcut_context_v1 *security_context = context_from_resource(resource);

    QObject::disconnect(CLIENT_CONNECT[resource]);
    CLIENT_CONNECT.erase(resource);

    context_destroy(security_context);
}

static void manager_handle_create_listener(struct wl_client *client,
                                           struct wl_resource *manager_resource,
                                           uint32_t id)
{

    struct treeland_shortcut_manager_v1 *manager = manager_from_resource(manager_resource);

    struct treeland_shortcut_context_v1 *context = static_cast<treeland_shortcut_context_v1*>(calloc(1, sizeof(*context)));
    if (context == NULL) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->manager = manager;

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource = wl_resource_create(client, &treeland_shortcut_context_v1_interface, version, id);
    if (resource == NULL) {
        free(context);
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    wl_resource_set_implementation(resource, &shortcut_context_impl, context, context_resource_destroy);

    wl_list_insert(&manager->contexts, &context->link);
}

static const struct treeland_shortcut_manager_v1_interface shortcut_manager_impl{
    .create = manager_handle_create_listener,
};

static struct treeland_shortcut_manager_v1 *manager_from_resource(
        struct wl_resource *resource) {
    assert(wl_resource_instance_of(resource, &treeland_shortcut_manager_v1_interface, &shortcut_manager_impl));
    struct treeland_shortcut_manager_v1 *manager = static_cast<treeland_shortcut_manager_v1*>(wl_resource_get_user_data(resource));
    assert(manager != NULL);
    return manager;
}

static void manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct treeland_shortcut_manager_v1 *manager = static_cast<treeland_shortcut_manager_v1*>(data);

    struct wl_resource *resource = wl_resource_create(client, &treeland_shortcut_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &shortcut_manager_impl, manager, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
    struct treeland_shortcut_manager_v1 *manager =
        wl_container_of(listener, manager, display_destroy);
    wl_signal_emit_mutable(&manager->events.destroy, manager);
    assert(wl_list_empty(&manager->events.destroy.listener_list));

    struct treeland_shortcut_context_v1 *context;
    struct treeland_shortcut_context_v1 *tmp;
    wl_list_for_each_safe(context, tmp, &manager->contexts, link) {
        context_destroy(context);
    }

    wl_global_destroy(manager->global);
    wl_list_remove(&manager->display_destroy.link);
    free(manager);
}

struct treeland_shortcut_manager_v1 *treeland_shortcut_manager_v1_create(struct wl_display *display, TreeLand::TreeLand *treeland, Helper *helper) {
    struct treeland_shortcut_manager_v1 *manager = static_cast<struct treeland_shortcut_manager_v1*>(calloc(1, sizeof(*manager)));
    if (!manager) {
        return nullptr;
    }

    manager->display = display;
    manager->treeland = treeland;
    manager->helper = helper;

    manager->global = wl_global_create(display, &treeland_shortcut_manager_v1_interface, TREELAND_SHORTCUT_MANAGER_V1_VERSION, manager, manager_bind);

    wl_list_init(&manager->contexts);

    wl_signal_init(&manager->events.destroy);
    wl_signal_init(&manager->events.shortcut);

    manager->display_destroy.notify = handle_display_destroy;
    wl_display_add_destroy_listener(display, &manager->display_destroy);

    return manager;
}

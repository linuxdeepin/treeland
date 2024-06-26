// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "window_management_impl.h"

#include <wayland-server-core.h>
#include <cassert>

#define TREELAND_WINDOW_MANAGEMENT_V1_VERSION 1

using QW_NAMESPACE::QWDisplay;

static treeland_window_management_v1 *treeland_window_management_from_resource(wl_resource *resource);
static void treeland_window_management_bind(wl_client *client, void *data, uint32_t version, uint32_t id);

static void treeland_window_management_handle_destroy(struct wl_client *, struct wl_resource *resource)
{
    wl_list_remove(&resource->link);
    wl_resource_destroy(resource);
}

void treeland_window_management_handle_set_desktop([[maybe_unused]]struct wl_client *client,
                                              struct wl_resource *resource,
                                              uint32_t state) {
    auto *manager = treeland_window_management_from_resource(resource);
    Q_EMIT manager->requestShowDesktop(state);
}

static const struct treeland_window_management_v1_interface window_management_impl {
    .set_desktop = treeland_window_management_handle_set_desktop,
    .destroy = treeland_window_management_handle_destroy,
};

treeland_window_management_v1::~treeland_window_management_v1()
{
    Q_EMIT beforeDestroy();
    if (global)
        wl_global_destroy(global);
}

treeland_window_management_v1 *treeland_window_management_v1::create(QWDisplay *display)
{
    auto *manager = new treeland_window_management_v1;
    if (!manager)
        return nullptr;

    manager->global = wl_global_create(display->handle(),
                                       &treeland_window_management_v1_interface,
                                       TREELAND_WINDOW_MANAGEMENT_V1_VERSION,
                                       manager,
                                       treeland_window_management_bind);

    if (!manager->global) {
        delete manager;
        return nullptr;
    }
    wl_list_init(&manager->resources);

    connect(display, &QWDisplay::beforeDestroy, manager, [manager] { delete manager; });
    return manager;
}

void treeland_window_management_v1::set_desktop(uint32_t state)
{
    this->state = state;
    wl_resource *resource;
    wl_list_for_each(resource, &this->resources, link)
    {
        treeland_window_management_v1_send_show_desktop(resource, state);
    }
}

static treeland_window_management_v1 *treeland_window_management_from_resource(wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_window_management_v1_interface,
                                   &window_management_impl));
    auto *manager = static_cast<treeland_window_management_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != nullptr);
    return manager;
}

static void treeland_window_management_resource_destroy(struct wl_resource *resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

static void treeland_window_management_bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto *manager = static_cast<treeland_window_management_v1 *>(data);

    struct wl_resource *resource =
        wl_resource_create(client, &treeland_window_management_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &window_management_impl,
                                   manager,
                                   treeland_window_management_resource_destroy);
    wl_list_insert(&manager->resources, wl_resource_get_link(resource));

    treeland_window_management_v1_send_show_desktop(resource, manager->state);
}

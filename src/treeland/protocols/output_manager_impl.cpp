// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "output_manager_impl.h"

#include "utils.h"

#include <wayland-server-core.h>

#include <cassert>

void output_manager_handle_set_primary_output([[maybe_unused]] struct wl_client *client,
                                              struct wl_resource *resource,
                                              const char *output)
{
    auto *manager = output_manager_from_resource(resource);
    wl_signal_emit(&manager->events.set_primary_output, &output);
}

static const struct treeland_output_manager_v1_interface output_manager_impl
{
    .set_primary_output = output_manager_handle_set_primary_output,
    .destroy = resource_handle_destroy,
};

struct treeland_output_manager_v1 *treeland_output_manager_v1_create(struct wl_display *display)
{
    auto *manager =
        static_cast<treeland_output_manager_v1 *>(calloc(1, sizeof(treeland_output_manager_v1)));
    if (manager == nullptr) {
        return nullptr;
    }
    manager->global = wl_global_create(display,
                                       &treeland_output_manager_v1_interface,
                                       TREELAND_OUTPUT_MANAGER_V1_VERSION,
                                       manager,
                                       output_manager_bind);

    wl_list_init(&manager->resources);
    wl_signal_init(&manager->events.set_primary_output);

    manager->display_destroy.notify = output_manager_handle_display_destroy;
    wl_display_add_destroy_listener(display, &manager->display_destroy);
    return manager;
}

void treeland_output_manager_v1_set_primary_output(treeland_output_manager_v1 *manager,
                                                   const char *name)
{
    manager->primary_output_name = name;
    wl_resource *resource;
    wl_list_for_each(resource, &manager->resources, link)
    {
        treeland_output_manager_v1_send_primary_output(resource, name);
    }
}

struct treeland_output_manager_v1 *output_manager_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_output_manager_v1_interface,
                                   &output_manager_impl));
    struct treeland_output_manager_v1 *manager =
        static_cast<treeland_output_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != NULL);
    return manager;
}

void output_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    struct treeland_output_manager_v1 *manager = static_cast<treeland_output_manager_v1 *>(data);

    struct wl_resource *resource =
        wl_resource_create(client, &treeland_output_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &output_manager_impl, manager, NULL);
    wl_list_insert(&manager->resources, wl_resource_get_link(resource));

    treeland_output_manager_v1_send_primary_output(resource, manager->primary_output_name);
}

void output_manager_handle_display_destroy(struct wl_listener *listener,
                                           [[maybe_unused]] void *data)
{
    struct treeland_output_manager_v1 *manager =
        wl_container_of(listener, manager, display_destroy);

    wl_global_destroy(manager->global);
    wl_list_remove(&manager->display_destroy.link);
    free(manager);
}

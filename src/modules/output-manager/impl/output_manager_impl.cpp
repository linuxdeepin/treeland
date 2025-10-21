// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "output_manager_impl.h"

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <cassert>

using QW_NAMESPACE::qw_display;

static treeland_output_manager_v1 *output_manager_from_resource(wl_resource *resource);
static void output_manager_bind(wl_client *client, void *data, uint32_t version, uint32_t id);
static void output_manager_handle_get_color_control(wl_client *client,
                                                    wl_resource *resource,
                                                    uint32_t id,
                                                    wl_resource *output_resource);
// For treeland_output_manager_v1_interface
static void output_manager_interface_handle_destroy(wl_client *, wl_resource *resource)
{
    wl_list_remove(&resource->link);
    wl_resource_destroy(resource);
}

static void output_manager_handle_set_primary_output([[maybe_unused]] wl_client *client,
                                                     wl_resource *resource,
                                                     const char *output)
{
    auto *manager = output_manager_from_resource(resource);
    Q_EMIT manager->requestSetPrimaryOutput(output);
}

static const struct treeland_output_manager_v1_interface output_manager_impl
{
    .set_primary_output = output_manager_handle_set_primary_output,
    .get_color_control = output_manager_handle_get_color_control,
    .destroy = output_manager_interface_handle_destroy,
};

// treeland output manager impl
treeland_output_manager_v1::~treeland_output_manager_v1()
{
    Q_EMIT before_destroy();
    // TODO: send stop to all clients
    wl_global_destroy(global);
}

treeland_output_manager_v1 *treeland_output_manager_v1::create(qw_display *display)
{
    auto *manager = new treeland_output_manager_v1;
    if (!manager) {
        return nullptr;
    }
    manager->global = wl_global_create(display->handle(),
                                       &treeland_output_manager_v1_interface,
                                       TREELAND_OUTPUT_MANAGER_V1_VERSION,
                                       manager,
                                       output_manager_bind);

    wl_list_init(&manager->resources);

    connect(display, &qw_display::before_destroy, manager, [manager] {
        delete manager;
    });

    return manager;
}

void treeland_output_manager_v1::set_primary_output(const char *name)
{
    this->primary_output_name = name;

    // Sometimes the whole resources list will be removed and leaved in an invalid state
    // (e.g. after a VT switching), so we need to ensure resources is valid before operations.
    wl_list_init(&this->resources);

    wl_resource *resource;
    wl_list_for_each(resource, &this->resources, link)
    {
        treeland_output_manager_v1_send_primary_output(resource, name);
    }
}

// static func
static treeland_output_manager_v1 *output_manager_from_resource(wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_output_manager_v1_interface,
                                   &output_manager_impl));
    auto *manager = static_cast<treeland_output_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != nullptr);
    return manager;
}

static void output_manager_resource_destroy(struct wl_resource *resource)
{
    // Safely remove resource from list
    auto link = wl_resource_get_link(resource);
    wl_list_init(link);
    wl_list_remove(link);
}

static void output_manager_bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto *manager = static_cast<treeland_output_manager_v1 *>(data);

    struct wl_resource *resource =
        wl_resource_create(client, &treeland_output_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &output_manager_impl,
                                   manager,
                                   output_manager_resource_destroy);
    wl_list_insert(&manager->resources, wl_resource_get_link(resource));

    treeland_output_manager_v1_send_primary_output(resource, manager->primary_output_name);
}

static void output_color_control_set_color_temperature([[maybe_unused]] struct wl_client *client,
                                                        struct wl_resource *resource,
                                                        uint32_t temperature)
{
    auto *color_control = treeland_output_color_control_v1::from_resource(resource);
    if (!color_control) {
        return;
    }

    Q_EMIT color_control->requestSetColorTemperature(temperature);
}

static void output_color_control_set_brightness([[maybe_unused]] struct wl_client *client,
                                                struct wl_resource *resource,
                                                uint32_t brightness)
{
    auto *color_control = treeland_output_color_control_v1::from_resource(resource);
    if (!color_control) {
        return;
    }

    Q_EMIT color_control->requestSetBrightness(brightness);
}

static void output_color_control_destroy([[maybe_unused]] struct wl_client *client,
                                        struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void output_color_control_resource_destroy(struct wl_resource *resource)
{
    if (!resource)
        return;

    auto color_control = treeland_output_color_control_v1::from_resource(resource);
    if (!color_control) {
        return;
    }

    delete color_control;
    wl_list_remove(wl_resource_get_link(resource));
}

struct treeland_output_color_control_v1_interface output_color_control_impl
{
    .set_color_temperature = output_color_control_set_color_temperature,
    .set_brightness = output_color_control_set_brightness,
    .destroy = output_color_control_destroy,
};

static void output_manager_handle_get_color_control(wl_client *client,
                                                    wl_resource *resource,
                                                    uint32_t id,
                                                    wl_resource *output_resource)
{
    auto *manager = output_manager_from_resource(resource);
    if (!manager) {
        return;
    }

    auto *wlr_output = wlr_output_from_resource(output_resource);
    if (!wlr_output) {
        return;
    }

    auto color_control = new treeland_output_color_control_v1;
    if (!color_control) {
        wl_resource_post_no_memory(resource);
        return;
    }

    uint32_t version = wl_resource_get_version(resource);
    struct wl_resource *color_resource =
        wl_resource_create(client,
                           &treeland_output_color_control_v1_interface,
                           version,
                           id);
    if (color_resource == nullptr) {
        delete color_control;
        wl_resource_post_no_memory(resource);
        return;
    }

    color_control->resource = color_resource;
    color_control->manager = manager;
    color_control->output = qw_output::from(wlr_output);

    wl_resource_set_implementation(color_resource,
                                   &output_color_control_impl,
                                   color_control,
                                   output_color_control_resource_destroy);
    wl_list_insert(&manager->resources, wl_resource_get_link(color_resource));
    
    Q_EMIT manager->colorControlCreated(color_control);
}

treeland_output_color_control_v1 *treeland_output_color_control_v1::from_resource(
    wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_output_color_control_v1_interface,
                                   &output_color_control_impl));
    return static_cast<struct treeland_output_color_control_v1 *>(
        wl_resource_get_user_data(resource));
}

void treeland_output_color_control_v1::sendColorTemperature(uint32_t temperature)
{
    treeland_output_color_control_v1_send_color_temperature(this->resource, temperature);
}

void treeland_output_color_control_v1::sendBrightness(uint32_t brightness)
{
    treeland_output_color_control_v1_send_brightness(this->resource, brightness);
}

treeland_output_color_control_v1::~treeland_output_color_control_v1()
{
    Q_EMIT before_destroy();
}

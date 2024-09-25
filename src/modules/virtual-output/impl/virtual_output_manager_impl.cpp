// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtual_output_manager_impl.h"

#include <wayland-server-core.h>

#include <QDebug>

#include <cassert>

#define TREELAND_VIRTUAL_OUTPUT_MANAGER_V1_VERSION 1

using QW_NAMESPACE::qw_display;

static treeland_virtual_output_manager_v1 *virtual_output_manager_from_resource(
    wl_resource *resource);
static void virtual_output_manager_bind(wl_client *client,
                                        void *data,
                                        uint32_t version,
                                        uint32_t id);

static void virtual_output_manager_handle_create_virtual_output(
    [[maybe_unused]] struct wl_client *client,
    struct wl_resource *manager_resource,
    uint32_t id,
    const char *name,
    struct wl_array *outputs);

static void virtual_output_manager_handle_get_virtual_output_list(
    [[maybe_unused]] struct wl_client *client, struct wl_resource *resource);

static void virtual_output_manager_handle_get_virtual_output(
    [[maybe_unused]] struct wl_client *client,
    struct wl_resource *resource,
    const char *name,
    uint32_t id);

static void virtual_output_handle_destroy([[maybe_unused]] struct wl_client *client,
                                          struct wl_resource *resource);

static const struct treeland_virtual_output_v1_interface virtual_output_impl
{
    .destroy = virtual_output_handle_destroy,
};

static const struct treeland_virtual_output_manager_v1_interface virtual_output_manager_impl
{
    .create_virtual_output = virtual_output_manager_handle_create_virtual_output,
    .get_virtual_output_list = virtual_output_manager_handle_get_virtual_output_list,
    .get_virtual_output = virtual_output_manager_handle_get_virtual_output,
};

static struct treeland_virtual_output_v1 *virtual_output_from_resource(wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_virtual_output_v1_interface,
                                   &virtual_output_impl));
    auto *manager = static_cast<treeland_virtual_output_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != nullptr);
    return manager;
}

static treeland_virtual_output_manager_v1 *virtual_output_manager_from_resource(
    wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_virtual_output_manager_v1_interface,
                                   &virtual_output_manager_impl));
    auto *manager =
        static_cast<treeland_virtual_output_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != nullptr);
    return manager;
}

static void virtual_output_handle_destroy([[maybe_unused]] struct wl_client *client,
                                          struct wl_resource *resource)
{

    auto *context = virtual_output_from_resource(resource);
    for (auto *virtual_output : context->manager->virtual_output) {
        if (resource == virtual_output->resource)
            Q_EMIT context->manager->virtualOutputDestroy(virtual_output);
    }
    wl_resource_destroy(resource);
}

static void virtual_output_resource_destroy(struct wl_resource *resource)
{
    auto *manager = virtual_output_from_resource(resource);
    if (!manager) {
        return;
    }

    delete manager;
}

static void virtual_output_manager_resource_destroy(struct wl_resource *resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

void wlarrayToStringList(const wl_array *wl_array, QStringList &stringList)
{
    char *dataStart = static_cast<char *>(wl_array->data);
    char *currentPos = dataStart;

    while (*currentPos != '\0') {
        QString str = QString::fromUtf8(currentPos);
        stringList << str;
        currentPos += str.toLocal8Bit().length() + 1;
    }
}

static void virtual_output_manager_handle_create_virtual_output(
    [[maybe_unused]] struct wl_client *client,
    struct wl_resource *manager_resource,
    uint32_t id,
    const char *name,
    struct wl_array *outputs)
{
    auto *manager = virtual_output_manager_from_resource(manager_resource);

    auto *virtual_output = new treeland_virtual_output_v1;
    if (virtual_output == nullptr) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_virtual_output_v1_interface, version, id);
    if (resource == nullptr) {
        delete virtual_output;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &virtual_output_impl,
                                   virtual_output,
                                   virtual_output_resource_destroy);
    wl_resource_set_user_data(resource, virtual_output);

    virtual_output->manager = manager;
    virtual_output->resource = resource;
    virtual_output->name = name;
    virtual_output->screen_outputs = outputs;

    wlarrayToStringList(outputs, virtual_output->outputList);

    manager->virtual_output.append(virtual_output);
    QObject::connect(virtual_output,
                     &treeland_virtual_output_v1::before_destroy,
                     manager,
                     [manager, virtual_output]() {
                         manager->virtual_output.removeOne(virtual_output);
                     });

    virtual_output->send_outputs(virtual_output->name, outputs);
    Q_EMIT virtual_output->manager->virtualOutputCreated(virtual_output);
}

static void virtual_output_manager_handle_get_virtual_output_list(
    [[maybe_unused]] struct wl_client *client, struct wl_resource *resource)
{
    auto *manager = virtual_output_manager_from_resource(resource);

    wl_array arr;
    wl_array_init(&arr);
    for (int i = 0; i < manager->virtual_output.size(); ++i) {
        treeland_virtual_output_v1 *virtual_output = manager->virtual_output.at(i);
        char *dest = static_cast<char *>(wl_array_add(&arr, virtual_output->name.length() + 1));
        strncpy(dest,
                virtual_output->name.toLatin1().data(),
                static_cast<uint>(virtual_output->name.length()));
    }

    treeland_virtual_output_manager_v1_send_virtual_output_list(resource, &arr);
}

static void virtual_output_manager_handle_get_virtual_output(
    [[maybe_unused]] struct wl_client *client,
    struct wl_resource *resource,
    const char *name,
    uint32_t id)
{
    auto *manager = virtual_output_manager_from_resource(resource);
    for (int i = 0; i < manager->virtual_output.size(); ++i) {
        treeland_virtual_output_v1 *virtual_output = manager->virtual_output.at(i);
        if (virtual_output->name == name) {
            virtual_output->send_outputs(name, virtual_output->screen_outputs);
        }
    }
}

void treeland_virtual_output_v1::send_outputs(QString name, struct wl_array *outputs)
{
    treeland_virtual_output_v1_send_outputs(resource, name.toLocal8Bit().data(), outputs);
}

void treeland_virtual_output_v1::send_error(uint32_t code, const char *message)
{
    treeland_virtual_output_v1_send_error(resource, code, message);
}

treeland_virtual_output_v1::~treeland_virtual_output_v1()
{
    Q_EMIT before_destroy();
    if (resource)
        wl_resource_set_user_data(resource, nullptr);
}

treeland_virtual_output_manager_v1::~treeland_virtual_output_manager_v1()
{
    Q_EMIT before_destroy();
    if (global)
        wl_global_destroy(global);
}

static void virtual_output_manager_bind(wl_client *client,
                                        void *data,
                                        uint32_t version,
                                        uint32_t id)
{
    auto *manager = static_cast<treeland_virtual_output_manager_v1 *>(data);

    struct wl_resource *resource =
        wl_resource_create(client, &treeland_virtual_output_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &virtual_output_manager_impl,
                                   manager,
                                   virtual_output_manager_resource_destroy);
    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
}

treeland_virtual_output_manager_v1 *treeland_virtual_output_manager_v1::create(qw_display *display)
{
    auto *manager = new treeland_virtual_output_manager_v1;
    if (!manager)
        return nullptr;

    manager->event_loop = wl_display_get_event_loop(display->handle());
    manager->global = wl_global_create(display->handle(),
                                       &treeland_virtual_output_manager_v1_interface,
                                       TREELAND_VIRTUAL_OUTPUT_MANAGER_V1_VERSION,
                                       manager,
                                       virtual_output_manager_bind);
    if (!manager->global) {
        delete manager;
        return nullptr;
    }

    wl_list_init(&manager->resources);

    connect(display, &qw_display::before_destroy, manager, [manager] {
        delete manager;
    });

    return manager;
}

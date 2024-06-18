// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "extforeigntoplevellist.h"

#include "server-protocol.h"

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>

#include <QDebug>
#include <QTimer>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
}

void ext_foreign_toplevel_list_v1::on_handleCreated(void *data)
{
    struct wl_resource *resource = static_cast<struct wl_resource *>(data);

    for (auto *surface : xdgSurfaces) {
        if (surface->surface()->handle()->handle()->resource == resource) {
            updateSurfaceInfo(surface);
            break;
        }
    }
}

ext_foreign_toplevel_list_v1::ext_foreign_toplevel_list_v1(ext_foreign_toplevel_list_v1 *handle,
                                                       bool isOwner)
    : QObject(nullptr)
{
}

ext_foreign_toplevel_list_v1::~ext_foreign_toplevel_list_v1()
{
    Q_EMIT beforeDestroy(this);
    struct ext_foreign_toplevel_handle_v1 *context;
    struct ext_foreign_toplevel_handle_v1 *tmp;
    for (auto handle : handles) {
        delete handle;
    }
    handles.clear();
    wl_global_destroy(m_global);
}

ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1::create(QWDisplay *display)
{
    auto *handle = new ext_foreign_toplevel_list_v1(nullptr,true);

    if (!handle->init(display)) {
        delete handle;
        return nullptr;
    }
    return handle;
}

bool ext_foreign_toplevel_list_v1::init(QWDisplay *display) {
    m_global = wl_global_create(display->handle(),
                                      &ext_foreign_toplevel_list_v1_interface,
                                      EXT_FOREIGN_TOPLEVEL_LIST_V1_TOPLEVEL_SINCE_VERSION,
                                      this,
                                      ext_foreign_toplevel_list_bind);
    if(!m_global)
        return false;
    connect(display, &QWDisplay::beforeDestroy, this, []{});
    return true;
}

void foreign_toplevel_list_handle_stop([[maybe_unused]] struct wl_client *client,
                                       struct wl_resource *resource);
void ext_foreign_toplevel_interface_destroy(struct wl_client *, struct wl_resource *resource);

static const struct ext_foreign_toplevel_list_v1_interface toplevel_list_impl
{
    .stop = foreign_toplevel_list_handle_stop, .destroy = ext_foreign_toplevel_interface_destroy
};

ext_foreign_toplevel_list_v1 *foreign_toplevel_list_from_resource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &ext_foreign_toplevel_list_v1_interface,
                                   &toplevel_list_impl));
    ext_foreign_toplevel_list_v1 *list =
        static_cast<ext_foreign_toplevel_list_v1 *>(wl_resource_get_user_data(resource));
    assert(list != NULL);
    return list;
}

void ext_foreign_toplevel_list_v1::ext_foreign_toplevel_list_v1_destroy(struct wl_resource *resource)
{
    auto *handle = foreign_toplevel_list_from_resource(resource);
    std::erase_if(handle->clients, [=](auto *c) {
        return c == resource;
    });
}

void ext_foreign_toplevel_list_v1::ext_foreign_toplevel_list_bind(struct wl_client *client,
                                    void *data,
                                    uint32_t version,
                                    uint32_t id)
{
    auto *handle = static_cast<ext_foreign_toplevel_list_v1 *>(data);

    struct wl_resource *resource =
        wl_resource_create(client, &ext_foreign_toplevel_list_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource,
                                   &::toplevel_list_impl,
                                   handle,
                                   ext_foreign_toplevel_list_v1_destroy);

    handle->clients.push_back(resource);

    // send all surface to client.
    for (auto *surface : handle->xdgSurfaces) {
        auto *toplevel = handle->createToplevelHandle(resource, surface->surface()->handle()->handle()->resource);
        ext_foreign_toplevel_list_v1_send_toplevel(resource, toplevel->resource);
        handle->on_handleCreated(surface->surface()->handle()->handle()->resource);
    }
}

void ext_foreign_toplevel_handle_destroy(struct wl_client *, struct wl_resource *resource);

static const struct ext_foreign_toplevel_handle_v1_interface toplevel_handle_impl
{
    .destroy = ext_foreign_toplevel_handle_destroy,
};

void ext_foreign_toplevel_handle_resource_destroy(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &ext_foreign_toplevel_handle_v1_interface,
                                   &toplevel_handle_impl));
    auto *handle = 
        static_cast<ext_foreign_toplevel_handle_v1 *>(wl_resource_get_user_data(resource));

    if (handle == nullptr) {
        return;
    }

    delete handle;
}

void ext_foreign_toplevel_handle_destroy(struct wl_client *, struct wl_resource *resource)
{
    ext_foreign_toplevel_handle_resource_destroy(resource);
}

void foreign_toplevel_list_handle_stop([[maybe_unused]] struct wl_client *client,
                                       struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &ext_foreign_toplevel_list_v1_interface,
                                   &toplevel_list_impl));
    [[maybe_unused]] auto *manager =
        foreign_toplevel_list_from_resource(resource);

    // TODO: stop
}

void ext_foreign_toplevel_interface_destroy(struct wl_client *, struct wl_resource *resource)
{
    wl_list_remove(&resource->link);
    wl_resource_destroy(resource);
}

ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_list_v1::createToplevelHandle(
    struct wl_resource *client,
    struct wl_resource *surface)
{
    auto *handle = new ext_foreign_toplevel_handle_v1;
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

    handles.push_back(handle);

    handle->manager = this;
    handle->resource = handle_resource;
    handle->surface = surface;

    return handle;
}

void ext_foreign_toplevel_list_v1::topLevel(Waylib::Server::WXdgSurface *surface)
{
    auto resource = surface->surface()->handle()->handle()->resource;
    xdgSurfaces.push_back(surface);

    // send surface to all clients.
    for (auto *client : clients) {
        auto *toplevel = this->createToplevelHandle(client, resource);
        ext_foreign_toplevel_list_v1_send_toplevel(client, toplevel->resource);
        on_handleCreated(resource);
    }
}

void ext_foreign_toplevel_list_v1::close(Waylib::Server::WXdgSurface *surface)
{
    std::erase_if(xdgSurfaces, [=](auto *s) {
        return s == surface;
    });

    auto resource = surface->surface()->handle()->handle()->resource;
    for (auto h : handles) {
        if (h->surface == resource){
            ext_foreign_toplevel_handle_v1_send_closed(h->resource);
            h->surface = nullptr;
        }
    }
}

void ext_foreign_toplevel_list_v1::done(Waylib::Server::WXdgSurface *surface)
{
    auto resource = surface->surface()->handle()->handle()->resource;
    for (auto h : handles)
    {
        if (h->surface == resource) {
            ext_foreign_toplevel_handle_v1_send_done(h->resource);
        }
    }
}

void ext_foreign_toplevel_list_v1::setTitle(Waylib::Server::WXdgSurface *surface,
                                          const QString &title)
{
    auto resource = surface->surface()->handle()->handle()->resource;
    for (auto h : handles)
    {
        if (h->surface == resource) {
            ext_foreign_toplevel_handle_v1_send_title(h->resource, title.toUtf8());
        }
    }
}

void ext_foreign_toplevel_list_v1::setAppId(Waylib::Server::WXdgSurface *surface,
                                          const QString &appId)
{
    auto resource = surface->surface()->handle()->handle()->resource;
    for (auto h : handles)
    {
        if (h->surface == resource) {
            ext_foreign_toplevel_handle_v1_send_app_id(h->resource, appId.toUtf8());
        }
    }
}

void ext_foreign_toplevel_list_v1::setIdentifier(Waylib::Server::WXdgSurface *surface,
                                               const QString &identifier)
{
    auto resource = surface->surface()->handle()->handle()->resource;
    for (auto h : handles)
    {
        if (h->surface == resource) {
            ext_foreign_toplevel_handle_v1_send_identifier(h->resource, identifier.toUtf8());
        }
    }
}

void ext_foreign_toplevel_list_v1::updateSurfaceInfo(Waylib::Server::WXdgSurface *surface)
{
    auto handle = [this, surface] {
        const QString &title = surface->title();
        if (title.isEmpty()) {
            return;
        }

        const QString &appId = surface->appId();
        if (appId.isEmpty()) {
            return;
        }

        setTitle(surface, title);
        setAppId(surface, appId);
        setIdentifier(surface, QString::number(reinterpret_cast<std::uintptr_t>(surface)));
        done(surface);
    };

    connect(surface, &Waylib::Server::WXdgSurface::titleChanged, this, handle);
    connect(surface, &Waylib::Server::WXdgSurface::appIdChanged, this, handle);

    handle();
}


ExtForeignToplevelList::ExtForeignToplevelList(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
{
}

void ExtForeignToplevelList::add(Waylib::Server::WXdgSurface *surface)
{
    manager->topLevel(surface);

    manager->updateSurfaceInfo(surface);
}

void ExtForeignToplevelList::remove(Waylib::Server::WXdgSurface *surface)
{
    manager->close(surface);
}

WServerInterface *ExtForeignToplevelList::create()
{
    manager.reset(ext_foreign_toplevel_list_v1::create(server()->handle()));
    return new WServerInterface(manager.data(), manager.data()->m_global);
}

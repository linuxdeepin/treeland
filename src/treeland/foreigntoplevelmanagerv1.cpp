// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreigntoplevelmanagerv1.h"

#include "foreign-toplevel-manager-server-protocol.h"
#include "helper.h"
#include "protocols/foreign_toplevel_manager_impl.h"
#include "treelandhelper.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>
#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>

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

class QWForeignToplevelManagerV1Private : public QWObjectPrivate
{
public:
    QWForeignToplevelManagerV1Private(ztreeland_foreign_toplevel_manager_v1 *handle,
                                      bool isOwner,
                                      QWForeignToplevelManagerV1 *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &QWForeignToplevelManagerV1Private::on_destroy);
        sc.connect(&handle->events.handleCreated,
                   this,
                   &QWForeignToplevelManagerV1Private::on_handleCreated);
    }

    ~QWForeignToplevelManagerV1Private()
    {
        if (!m_handle)
            return;
        destroy();
        if (isHandleOwner)
            ztreeland_foreign_toplevel_manager_v1_destroy(q_func()->handle());
    }

    inline void destroy()
    {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        Q_EMIT q_func()->beforeDestroy(q_func());
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);
    void on_handleCreated(void *);

    static QHash<void *, QWForeignToplevelManagerV1 *> map;
    QW_DECLARE_PUBLIC(QWForeignToplevelManagerV1)
    QWSignalConnector sc;

    std::vector<Waylib::Server::WXdgSurface *> xdgSurfaces;
};

QHash<void *, QWForeignToplevelManagerV1 *> QWForeignToplevelManagerV1Private::map;

void QWForeignToplevelManagerV1Private::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void QWForeignToplevelManagerV1Private::on_handleCreated(void *data)
{
    struct wl_resource *resource = static_cast<struct wl_resource *>(data);

    for (auto *surface : xdgSurfaces) {
        if (surface->surface()->handle()->handle()->resource == resource) {
            q_func()->updateSurfaceInfo(surface);
            break;
        }
    }
}

QWForeignToplevelManagerV1::QWForeignToplevelManagerV1(
    ztreeland_foreign_toplevel_manager_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new QWForeignToplevelManagerV1Private(handle, isOwner, this))
{
}

QWForeignToplevelManagerV1 *
QWForeignToplevelManagerV1::get(ztreeland_foreign_toplevel_manager_v1 *handle)
{
    return QWForeignToplevelManagerV1Private::map.value(handle);
}

QWForeignToplevelManagerV1 *
QWForeignToplevelManagerV1::from(ztreeland_foreign_toplevel_manager_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new QWForeignToplevelManagerV1(handle, false);
}

QWForeignToplevelManagerV1 *QWForeignToplevelManagerV1::create(QWDisplay *display)
{
    auto *handle = ztreeland_foreign_toplevel_manager_v1_create(display->handle());
    if (!handle)
        return nullptr;
    return new QWForeignToplevelManagerV1(handle, true);
}

void QWForeignToplevelManagerV1::topLevel(Waylib::Server::WXdgSurface *surface)
{
    d_func()->xdgSurfaces.push_back(surface);

    ztreeland_foreign_toplevel_manager_v1_toplevel(
        handle(),
        surface->surface()->handle()->handle()->resource);
}

void QWForeignToplevelManagerV1::close(Waylib::Server::WXdgSurface *surface)
{
    std::erase_if(d_func()->xdgSurfaces, [=](auto *s) {
        return s == surface;
    });

    ztreeland_foreign_toplevel_handle_v1_closed(handle(),
                                                surface->surface()->handle()->handle()->resource);
}

void QWForeignToplevelManagerV1::done(Waylib::Server::WXdgSurface *surface)
{
    ztreeland_foreign_toplevel_handle_v1_done(handle(),
                                              surface->surface()->handle()->handle()->resource);
}

void QWForeignToplevelManagerV1::setPid(Waylib::Server::WXdgSurface *surface, uint32_t pid)
{
    ztreeland_foreign_toplevel_handle_v1_pid(handle(),
                                             surface->surface()->handle()->handle()->resource,
                                             pid);
}

void QWForeignToplevelManagerV1::setIdentifier(Waylib::Server::WXdgSurface *surface,
                                               const QString &identifier)
{
    ztreeland_foreign_toplevel_handle_v1_identifier(
        handle(),
        surface->surface()->handle()->handle()->resource,
        identifier);
}

void QWForeignToplevelManagerV1::updateSurfaceInfo(Waylib::Server::WXdgSurface *surface)
{
    {
        wl_client *client = surface->surface()->handle()->handle()->resource->client;
        pid_t pid;
        wl_client_get_credentials(client, &pid, nullptr, nullptr);
        setPid(surface, pid);
    }

    setIdentifier(surface, QString::number(reinterpret_cast<std::uintptr_t>(surface)));
    done(surface);
}

class ForeignToplevelManagerPrivate : public WObjectPrivate
{
public:
    ForeignToplevelManagerPrivate(ForeignToplevelManager *qq)
        : WObjectPrivate(qq)
    {
    }

    W_DECLARE_PUBLIC(ForeignToplevelManager)

    QWForeignToplevelManagerV1 *manager = nullptr;
};

ForeignToplevelManager::ForeignToplevelManager(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
    , Waylib::Server::WObject(*new ForeignToplevelManagerPrivate(this), nullptr)
{
}

void ForeignToplevelManager::add(Waylib::Server::WXdgSurface *surface)
{
    W_D(ForeignToplevelManager);

    d->manager->topLevel(surface);

    d->manager->updateSurfaceInfo(surface);
}

void ForeignToplevelManager::remove(Waylib::Server::WXdgSurface *surface)
{
    W_D(ForeignToplevelManager);

    d->manager->close(surface);
}

void ForeignToplevelManager::create()
{
    W_D(ForeignToplevelManager);
    WQuickWaylandServerInterface::create();

    d->manager = QWForeignToplevelManagerV1::create(server()->handle());
}

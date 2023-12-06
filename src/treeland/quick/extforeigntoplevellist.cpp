// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "extforeigntoplevellist.h"

#include "ext-foreign-toplevel-list-server-protocol.h"
#include "protocols/ext_foreign_toplevel_list_impl.h"

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

class QWExtForeignToplevelListV1Private : public QWObjectPrivate
{
public:
    QWExtForeignToplevelListV1Private(ext_foreign_toplevel_list_v1 *handle,
                                      bool isOwner,
                                      QWExtForeignToplevelListV1 *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &QWExtForeignToplevelListV1Private::on_destroy);
        sc.connect(&handle->events.handleCreated,
                   this,
                   &QWExtForeignToplevelListV1Private::on_handleCreated);
    }

    ~QWExtForeignToplevelListV1Private()
    {
        if (!m_handle)
            return;
        destroy();
        if (isHandleOwner)
            ext_foreign_toplevel_list_v1_destroy(q_func()->handle());
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

    static QHash<void *, QWExtForeignToplevelListV1 *> map;
    QW_DECLARE_PUBLIC(QWExtForeignToplevelListV1)
    QWSignalConnector sc;

    std::vector<Waylib::Server::WXdgSurface *> xdgSurfaces;
};

QHash<void *, QWExtForeignToplevelListV1 *> QWExtForeignToplevelListV1Private::map;

void QWExtForeignToplevelListV1Private::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void QWExtForeignToplevelListV1Private::on_handleCreated(void *data)
{
    struct wl_resource *resource = static_cast<struct wl_resource *>(data);

    for (auto *surface : xdgSurfaces) {
        if (surface->surface()->handle()->handle()->resource == resource) {
            q_func()->updateSurfaceInfo(surface);
            break;
        }
    }
}

QWExtForeignToplevelListV1::QWExtForeignToplevelListV1(ext_foreign_toplevel_list_v1 *handle,
                                                       bool isOwner)
    : QObject(nullptr)
    , QWObject(*new QWExtForeignToplevelListV1Private(handle, isOwner, this))
{
}

QWExtForeignToplevelListV1 *QWExtForeignToplevelListV1::get(ext_foreign_toplevel_list_v1 *handle)
{
    return QWExtForeignToplevelListV1Private::map.value(handle);
}

QWExtForeignToplevelListV1 *QWExtForeignToplevelListV1::from(ext_foreign_toplevel_list_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new QWExtForeignToplevelListV1(handle, false);
}

QWExtForeignToplevelListV1 *QWExtForeignToplevelListV1::create(QWDisplay *display)
{
    auto *handle = ext_foreign_toplevel_list_v1_create(display->handle());
    if (!handle)
        return nullptr;
    return new QWExtForeignToplevelListV1(handle, true);
}

void QWExtForeignToplevelListV1::topLevel(Waylib::Server::WXdgSurface *surface)
{
    d_func()->xdgSurfaces.push_back(surface);

    ext_foreign_toplevel_list_v1_toplevel(handle(),
                                          surface->surface()->handle()->handle()->resource);
}

void QWExtForeignToplevelListV1::close(Waylib::Server::WXdgSurface *surface)
{
    std::erase_if(d_func()->xdgSurfaces, [=](auto *s) {
        return s == surface;
    });

    ext_foreign_toplevel_handle_v1_closed(handle(),
                                          surface->surface()->handle()->handle()->resource);
}

void QWExtForeignToplevelListV1::done(Waylib::Server::WXdgSurface *surface)
{
    ext_foreign_toplevel_handle_v1_done(handle(), surface->surface()->handle()->handle()->resource);
}

void QWExtForeignToplevelListV1::setTitle(Waylib::Server::WXdgSurface *surface,
                                          const QString &title)
{
    ext_foreign_toplevel_handle_v1_title(handle(),
                                         surface->surface()->handle()->handle()->resource,
                                         title);
}

void QWExtForeignToplevelListV1::setAppId(Waylib::Server::WXdgSurface *surface,
                                          const QString &appId)
{
    ext_foreign_toplevel_handle_v1_app_id(handle(),
                                          surface->surface()->handle()->handle()->resource,
                                          appId);
}

void QWExtForeignToplevelListV1::setIdentifier(Waylib::Server::WXdgSurface *surface,
                                               const QString &identifier)
{
    ext_foreign_toplevel_handle_v1_identifier(handle(),
                                              surface->surface()->handle()->handle()->resource,
                                              identifier);
}

void QWExtForeignToplevelListV1::updateSurfaceInfo(Waylib::Server::WXdgSurface *surface)
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

class ExtForeignToplevelListPrivate : public WObjectPrivate
{
public:
    ExtForeignToplevelListPrivate(ExtForeignToplevelList *qq)
        : WObjectPrivate(qq)
    {
    }

    W_DECLARE_PUBLIC(ExtForeignToplevelList)

    QWExtForeignToplevelListV1 *manager = nullptr;
};

ExtForeignToplevelList::ExtForeignToplevelList(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
    , Waylib::Server::WObject(*new ExtForeignToplevelListPrivate(this), nullptr)
{
}

void ExtForeignToplevelList::add(Waylib::Server::WXdgSurface *surface)
{
    W_D(ExtForeignToplevelList);

    d->manager->topLevel(surface);

    d->manager->updateSurfaceInfo(surface);
}

void ExtForeignToplevelList::remove(Waylib::Server::WXdgSurface *surface)
{
    W_D(ExtForeignToplevelList);

    d->manager->close(surface);
}

void ExtForeignToplevelList::create()
{
    W_D(ExtForeignToplevelList);
    WQuickWaylandServerInterface::create();

    d->manager = QWExtForeignToplevelListV1::create(server()->handle());
}

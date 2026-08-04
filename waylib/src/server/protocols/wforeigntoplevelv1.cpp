// Copyright (C) 2023-2026 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wforeigntoplevelv1.h"

#include "private/wglobal_p.h"
#include "woutput.h"
#include "wtoplevelsurface.h"
#include "wxdgtoplevelsurface.h"
#include "wxwaylandsurface.h"
#include "wayliblogging.h"

extern "C" {
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
}

#include <map>
#include <memory>

WAYLIB_SERVER_BEGIN_NAMESPACE

struct Q_DECL_HIDDEN ForeignToplevelEntry
{
    void disconnectNativeEvents()
    {
        for (auto *listener : { &requestActivate, &requestMaximize, &requestMinimize,
                                &requestFullscreen, &requestClose, &setRectangle }) {
            listener->disconnect();
        }
    }

    wlr_foreign_toplevel_handle_v1 *handle = nullptr;
    QList<QMetaObject::Connection> surfaceConnections;
    WNativeListener requestActivate;
    WNativeListener requestMaximize;
    WNativeListener requestMinimize;
    WNativeListener requestFullscreen;
    WNativeListener requestClose;
    WNativeListener setRectangle;
};

class Q_DECL_HIDDEN WForeignToplevelPrivate : public WObjectPrivate
{
public:
    explicit WForeignToplevelPrivate(WForeignToplevel *qq)
        : WObjectPrivate(qq)
    {
    }

    void initSurface(WToplevelSurface *surface, ForeignToplevelEntry *entry)
    {
        W_Q(WForeignToplevel);
        auto *handle = entry->handle;
        auto connectSurface = [entry](const QMetaObject::Connection &connection) {
            entry->surfaceConnections.append(connection);
        };

        connectSurface(surface->safeConnect(&WToplevelSurface::titleChanged, q, [handle, surface] {
            const auto title = surface->title().toUtf8();
            wlr_foreign_toplevel_handle_v1_set_title(handle, title.constData());
        }));
        connectSurface(surface->safeConnect(&WToplevelSurface::appIdChanged, q, [handle, surface] {
            const auto appId = surface->appId().toLatin1();
            wlr_foreign_toplevel_handle_v1_set_app_id(handle, appId.constData());
        }));
        connectSurface(surface->safeConnect(&WToplevelSurface::minimizeChanged, q, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_minimized(handle, surface->isMinimized());
        }));
        connectSurface(surface->safeConnect(&WToplevelSurface::maximizeChanged, q, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_maximized(handle, surface->isMaximized());
        }));
        connectSurface(surface->safeConnect(&WToplevelSurface::fullscreenChanged, q, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_fullscreen(handle, surface->isFullScreen());
        }));
        connectSurface(surface->safeConnect(&WToplevelSurface::activateChanged, q, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_activated(handle, surface->isActivated());
        }));

        if (auto *xdgSurface = qobject_cast<WXdgToplevelSurface *>(surface)) {
            auto updateParent = [this, handle, xdgSurface] {
                auto *parent = xdgSurface->parentXdgSurface();
                if (!parent) {
                    wlr_foreign_toplevel_handle_v1_set_parent(handle, nullptr);
                    return;
                }
                const auto it = surfaces.find(parent);
                if (it == surfaces.end()) {
                    qCCritical(lcWlForeignToplevel)
                        << "Xdg toplevel surface" << xdgSurface
                        << "has set parent surface, but foreign_toplevel_handle for parent surface not found!";
                    return;
                }
                wlr_foreign_toplevel_handle_v1_set_parent(handle, it->second->handle);
            };
            connectSurface(xdgSurface->safeConnect(
                &WXdgToplevelSurface::parentXdgSurfaceChanged, q, updateParent));
            updateParent();
        } else if (auto *xwaylandSurface = qobject_cast<WXWaylandSurface *>(surface)) {
            auto updateParent = [this, handle, xwaylandSurface] {
                auto *parent = xwaylandSurface->parentXWaylandSurface();
                if (!parent) {
                    wlr_foreign_toplevel_handle_v1_set_parent(handle, nullptr);
                    return;
                }
                const auto it = surfaces.find(parent);
                if (it == surfaces.end()) {
                    qCCritical(lcWlForeignToplevel)
                        << "X11 surface" << xwaylandSurface
                        << "has set parent surface, but foreign_toplevel_handle for parent surface not found!";
                    return;
                }
                wlr_foreign_toplevel_handle_v1_set_parent(handle, it->second->handle);
            };
            connectSurface(xwaylandSurface->safeConnect(
                &WXWaylandSurface::parentXWaylandSurfaceChanged, q, updateParent));
            updateParent();
        }

        connectSurface(surface->surface()->safeConnect(&WSurface::outputEntered, q, [handle](WOutput *output) {
            wlr_foreign_toplevel_handle_v1_output_enter(handle, output->handle());
        }));
        connectSurface(surface->surface()->safeConnect(&WSurface::outputLeave, q, [handle](WOutput *output) {
            wlr_foreign_toplevel_handle_v1_output_leave(handle, output->handle());
        }));

        entry->requestActivate.connect(&handle->events.request_activate, [q, surface](void *) {
            Q_EMIT q->requestActivate(surface);
        });
        entry->requestMaximize.connect(&handle->events.request_maximize, [q, surface](void *data) {
            const auto *event = static_cast<wlr_foreign_toplevel_handle_v1_maximized_event *>(data);
            Q_EMIT q->requestMaximize(surface, event->maximized);
        });
        entry->requestMinimize.connect(&handle->events.request_minimize, [q, surface](void *data) {
            const auto *event = static_cast<wlr_foreign_toplevel_handle_v1_minimized_event *>(data);
            Q_EMIT q->requestMinimize(surface, event->minimized);
        });
        entry->requestFullscreen.connect(&handle->events.request_fullscreen, [q, surface](void *data) {
            const auto *event = static_cast<wlr_foreign_toplevel_handle_v1_fullscreen_event *>(data);
            Q_EMIT q->requestFullscreen(surface, event->fullscreen);
        });
        entry->requestClose.connect(&handle->events.request_close, [q, surface](void *) {
            Q_EMIT q->requestClose(surface);
        });
        entry->setRectangle.connect(&handle->events.set_rectangle, [q, surface](void *data) {
            const auto *event = static_cast<wlr_foreign_toplevel_handle_v1_set_rectangle_event *>(data);
            Q_EMIT q->rectangleChanged(
                surface, QRect { event->x, event->y, event->width, event->height });
        });

        const auto title = surface->title().toUtf8();
        const auto appId = surface->appId().toLatin1();
        wlr_foreign_toplevel_handle_v1_set_title(handle, title.constData());
        wlr_foreign_toplevel_handle_v1_set_app_id(handle, appId.constData());
        wlr_foreign_toplevel_handle_v1_set_minimized(handle, surface->isMinimized());
        wlr_foreign_toplevel_handle_v1_set_maximized(handle, surface->isMaximized());
        wlr_foreign_toplevel_handle_v1_set_fullscreen(handle, surface->isFullScreen());
        wlr_foreign_toplevel_handle_v1_set_activated(handle, surface->isActivated());
    }

    void add(WToplevelSurface *surface)
    {
        W_Q(WForeignToplevel);
        if (surfaces.contains(surface)) {
            qCCritical(lcWlForeignToplevel)
                << surface << " has been add to foreign toplevel twice";
            return;
        }

        auto entry = std::make_unique<ForeignToplevelEntry>();
        entry->handle = wlr_foreign_toplevel_handle_v1_create(q->handle());
        Q_ASSERT(entry->handle);
        auto *entryPtr = entry.get();
        surfaces.emplace(surface, std::move(entry));
        initSurface(surface, entryPtr);
    }

    void remove(WToplevelSurface *surface)
    {
        const auto it = surfaces.find(surface);
        if (it == surfaces.end())
            return;
        for (const auto &connection : std::as_const(it->second->surfaceConnections))
            QObject::disconnect(connection);
        it->second->disconnectNativeEvents();
        wlr_foreign_toplevel_handle_v1_destroy(it->second->handle);
        surfaces.erase(it);
    }

    void clear()
    {
        while (!surfaces.empty())
            remove(surfaces.begin()->first);
    }

    W_DECLARE_PUBLIC(WForeignToplevel)

    std::map<WToplevelSurface *, std::unique_ptr<ForeignToplevelEntry>> surfaces;
};

WForeignToplevel::WForeignToplevel(QObject *parent)
    : QObject(parent)
    , WObject(*new WForeignToplevelPrivate(this))
{
}

void WForeignToplevel::addSurface(WToplevelSurface *surface)
{
    d_func()->add(surface);
}

void WForeignToplevel::removeSurface(WToplevelSurface *surface)
{
    d_func()->remove(surface);
}

wlr_foreign_toplevel_manager_v1 *WForeignToplevel::handle() const
{
    return nativeInterface<wlr_foreign_toplevel_manager_v1>();
}

QByteArrayView WForeignToplevel::interfaceName() const
{
    return "zwlr_foreign_toplevel_manager_v1";
}

void WForeignToplevel::create(WServer *server)
{
    m_handle = wlr_foreign_toplevel_manager_v1_create(server->handle());
    Q_ASSERT(m_handle);
}

void WForeignToplevel::destroy([[maybe_unused]] WServer *server)
{
    d_func()->clear();
    m_handle = nullptr;
}

wl_global *WForeignToplevel::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE

// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wforeigntoplevelv1.h"

#include "private/wglobal_p.h"
#include "wglobal.h"
#include "wscoplistener.h"
#include "woutput.h"
#include "wtoplevelsurface.h"
#include "wxdgtoplevelsurface.h"
#include "wxwaylandsurface.h"
#include "wayliblogging.h"

#include <wlr_all.h>

#include <wpointer.h>

#include <map>

WAYLIB_SERVER_BEGIN_NAMESPACE
class Q_DECL_HIDDEN WForeignToplevelPrivate : public WObjectPrivate
{
public:
    WForeignToplevelPrivate(WForeignToplevel *qq)
        : WObjectPrivate(qq)
    {
    }

    ~WForeignToplevelPrivate() {
        // The Qt connections live on the surfaces (receiver) while the
        // lambdas capture this private and the raw handles; the manager may
        // be destroyed before the surfaces, so disconnect them all here
        // (mirrors remove()).
        for (const auto &conns : std::as_const(surfaceConnections)) {
            for (const auto &c : conns)
                QObject::disconnect(c);
        }
    }

    void initSurface(WToplevelSurface *surface)
    {
        W_Q(WForeignToplevel);
        auto handle = surfaces.at(surface).get();

        QList<QMetaObject::Connection> conns;
        conns.append(QObject::connect(surface, &WToplevelSurface::titleChanged, surface, [handle, surface] {
            const auto title = surface->title().toUtf8();
            wlr_foreign_toplevel_handle_v1_set_title(handle, title);
        }));

        conns.append(QObject::connect(surface, &WToplevelSurface::appIdChanged, surface, [handle, surface] {
            const auto appId = surface->appId().toLatin1();
            wlr_foreign_toplevel_handle_v1_set_app_id(handle, appId);
        }));

        conns.append(QObject::connect(surface, &WToplevelSurface::minimizeChanged, surface, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_minimized(handle, surface->isMinimized());
        }));

        conns.append(QObject::connect(surface, &WToplevelSurface::maximizeChanged, surface, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_maximized(handle, surface->isMaximized());
        }));

        conns.append(QObject::connect(surface, &WToplevelSurface::fullscreenChanged, surface, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_fullscreen(handle, surface->isFullScreen());
        }));

        conns.append(QObject::connect(surface, &WToplevelSurface::activateChanged, surface, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_activated(handle, surface->isActivated());
        }));

        if (auto *xdgSurface = qobject_cast<WXdgToplevelSurface *>(surface)) {
            auto updateSurfaceParent = [this, handle, xdgSurface] {
                WToplevelSurface *p = xdgSurface->parentXdgSurface();
                if (!p) {
                    wlr_foreign_toplevel_handle_v1_set_parent(handle, nullptr);
                    return;
                }
                if (!surfaces.contains(p)) {
                    qCCritical(lcWlForeignToplevel)
                        << "Xdg toplevel surface " << xdgSurface
                        << "has set parent surface, but foreign_toplevel_handle for parent surface "
                           "not found!";
                    return;
                }
                wlr_foreign_toplevel_handle_v1_set_parent(handle, surfaces.at(p).get());
            };
            conns.append(QObject::connect(xdgSurface, &WToplevelSurface::parentSurfaceChanged,
                                                 surface,
                                                 updateSurfaceParent));
            updateSurfaceParent();
        } else if (auto *xwaylandSurface = qobject_cast<WXWaylandSurface *>(surface)) {
            auto updateSurfaceParent = [this, handle, xwaylandSurface] {
                WToplevelSurface *p = xwaylandSurface->parentXWaylandSurface();
                if (!p) {
                    wlr_foreign_toplevel_handle_v1_set_parent(handle, nullptr);
                    return;
                }
                if (!surfaces.contains(p)) {
                    qCCritical(lcWlForeignToplevel)
                        << "X11 surface " << xwaylandSurface
                        << "has set parent surface, but foreign_toplevel_handle for parent surface "
                           "not found!";
                    return;
                }
                wlr_foreign_toplevel_handle_v1_set_parent(handle, surfaces.at(p).get());
            };
            conns.append(QObject::connect(xwaylandSurface, &WToplevelSurface::parentSurfaceChanged,
                                                     surface,
                                                     updateSurfaceParent));
            updateSurfaceParent();
        }

        conns.append(QObject::connect(surface->surface(), &WSurface::outputEntered,
                                                     surface,
                                                     [handle](WOutput *output) {
                                                         wlr_foreign_toplevel_handle_v1_output_enter(handle, output->handle());
                                                     }));

        conns.append(QObject::connect(surface->surface(), &WSurface::outputLeave,
                                                     surface,
                                                     [handle](WOutput *output) {
                                                         wlr_foreign_toplevel_handle_v1_output_leave(handle, output->handle());
                                                     }));

        auto *wsurface = surface->surface();
        auto *ls = wsurface->listeners(q_ptr);
        ls->add(&handle->events.request_activate, surface,
                [surface, q]([[maybe_unused]] wlr_foreign_toplevel_handle_v1_activated_event *event) {
                    Q_EMIT q->requestActivate(surface);
                });
        ls->add(&handle->events.request_maximize, surface,
                [surface, q](wlr_foreign_toplevel_handle_v1_maximized_event *event) {
                    Q_EMIT q->requestMaximize(surface, event->maximized);
                });
        ls->add(&handle->events.request_minimize, surface,
                [surface, q](wlr_foreign_toplevel_handle_v1_minimized_event *event) {
                    Q_EMIT q->requestMinimize(surface, event->minimized);
                });
        ls->add(&handle->events.request_fullscreen, surface,
                [surface, q](wlr_foreign_toplevel_handle_v1_fullscreen_event *event) {
                    Q_EMIT q->requestFullscreen(surface, event->fullscreen);
                });
        ls->add(&handle->events.request_close, surface,
                [surface, q](void *) {
                    Q_EMIT q->requestClose(surface);
                });
        ls->add(&handle->events.set_rectangle, surface,
                [surface, q](wlr_foreign_toplevel_handle_v1_set_rectangle_event *event) {
                    Q_EMIT q->rectangleChanged(
                        surface,
                        QRect{ event->x, event->y, event->width, event->height });
                });

        const auto title = surface->title().toUtf8();
        const auto appId = surface->appId().toLatin1();
        wlr_foreign_toplevel_handle_v1_set_title(handle, title);
        wlr_foreign_toplevel_handle_v1_set_app_id(handle, appId);
        wlr_foreign_toplevel_handle_v1_set_minimized(handle, surface->isMinimized());
        wlr_foreign_toplevel_handle_v1_set_maximized(handle, surface->isMaximized());
        wlr_foreign_toplevel_handle_v1_set_fullscreen(handle, surface->isFullScreen());
        wlr_foreign_toplevel_handle_v1_set_activated(handle, surface->isActivated());
        surfaceConnections.insert(handle, conns);
    }

    void add(WToplevelSurface *surface)
    {
        W_Q(WForeignToplevel);

        if (surfaces.contains(surface)) {
            qCCritical(lcWlForeignToplevel)
                << surface << " has been add to foreign toplevel twice";
            return;
        }

        auto handle = wlr_foreign_toplevel_handle_v1_create(
            reinterpret_cast<wlr_foreign_toplevel_manager_v1*>(q->m_handle));
        surfaces.insert({ surface, WUniquePointer<wlr_foreign_toplevel_handle_v1>(handle) });
        initSurface(surface);
    }

    void remove(WToplevelSurface *surface)
    {
        auto it = surfaces.find(surface);
        if (it != surfaces.end()) {
            surface->surface()->removeListeners(q_ptr);
            // The Qt connections below live on the surface (receiver), while the
            // lambdas capture the raw wlr handle; disconnect them before the
            // handle is freed, mirroring master where the qw_* handle QObject
            // teardown disconnected them implicitly.
            const auto conns = surfaceConnections.take(it->second.get());
            for (const auto &c : conns)
                QObject::disconnect(c);
        }
        surfaces.erase(surface);
    }

    QHash<wlr_foreign_toplevel_handle_v1 *, QList<QMetaObject::Connection>> surfaceConnections;

    W_DECLARE_PUBLIC(WForeignToplevel)

    std::map<WToplevelSurface *, WUniquePointer<wlr_foreign_toplevel_handle_v1>> surfaces;
};

WForeignToplevel::WForeignToplevel([[maybe_unused]] QObject *parent)
    : WObject(*new WForeignToplevelPrivate(this), nullptr)
{
}

void WForeignToplevel::addSurface(WToplevelSurface *surface)
{
    W_D(WForeignToplevel);

    d->add(surface);
}

void WForeignToplevel::removeSurface(WToplevelSurface *surface)
{
    W_D(WForeignToplevel);

    d->remove(surface);
}

QByteArrayView WForeignToplevel::interfaceName() const
{
    return "zwlr_foreign_toplevel_manager_v1";
}

wlr_foreign_toplevel_manager_v1 *WForeignToplevel::handle() const
{
    return reinterpret_cast<wlr_foreign_toplevel_manager_v1*>(m_handle);
}

void WForeignToplevel::create(WServer *server)
{
    m_handle = wlr_foreign_toplevel_manager_v1_create(server->handle());
}

void WForeignToplevel::destroy([[maybe_unused]] WServer *server)
{
    // The wlr_foreign_toplevel_manager_v1 is reclaimed by display.reset() in
    // WServer::stop(); null m_handle so handle()/global() return null instead
    // of a dangling pointer (kept as an explicit override rather than the
    // inherited empty base for this hardening).
    m_handle = nullptr;
}

wl_global *WForeignToplevel::global() const
{
    if (!m_handle)
        return nullptr;
    return reinterpret_cast<wlr_foreign_toplevel_manager_v1*>(m_handle)->global;
}

WAYLIB_SERVER_END_NAMESPACE

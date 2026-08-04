// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wforeigntoplevelv1.h"

#include "private/wglobal_p.h"
#include "wglobal.h"
#include "woutput.h"
#include "wtoplevelsurface.h"
#include "wxdgtoplevelsurface.h"
#include "wxwaylandsurface.h"
#include "wayliblogging.h"

#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

#include <map>

WAYLIB_SERVER_BEGIN_NAMESPACE
class Q_DECL_HIDDEN WForeignToplevelPrivate : public WObjectPrivate
{
public:
    WForeignToplevelPrivate(WForeignToplevel *qq)
        : WObjectPrivate(qq)
    {
    }

    void initSurface(WToplevelSurface *surface, HandleData *data)
    {
        W_Q(WForeignToplevel);
        auto *handle = data->handle.get();
        surface->safeConnect(&WToplevelSurface::titleChanged, q, [handle, surface] {
            const auto title = surface->title().toUtf8();
            wlr_foreign_toplevel_handle_v1_set_title(handle, title);
        });

        surface->safeConnect(&WToplevelSurface::appIdChanged, q, [handle, surface] {
            const auto appId = surface->appId().toLatin1();
            wlr_foreign_toplevel_handle_v1_set_app_id(handle, appId);
        });

        surface->safeConnect(&WToplevelSurface::minimizeChanged, q, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_minimized(handle, surface->isMinimized());
        });

        surface->safeConnect(&WToplevelSurface::maximizeChanged, q, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_maximized(handle, surface->isMaximized());
        });

        surface->safeConnect(&WToplevelSurface::fullscreenChanged, q, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_fullscreen(handle, surface->isFullScreen());
        });

        surface->safeConnect(&WToplevelSurface::activateChanged, q, [handle, surface] {
            wlr_foreign_toplevel_handle_v1_set_activated(handle, surface->isActivated());
        });

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
                wlr_foreign_toplevel_handle_v1_set_parent(handle, surfaces.at(p)->handle.get());
            };
            xdgSurface->safeConnect(&WXdgToplevelSurface::parentXdgSurfaceChanged,
                                    q,
                                    updateSurfaceParent);
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
                wlr_foreign_toplevel_handle_v1_set_parent(handle, surfaces.at(p)->handle.get());
            };
            xwaylandSurface->safeConnect(&WXWaylandSurface::parentXWaylandSurfaceChanged,
                                         q,
                                         updateSurfaceParent);
            updateSurfaceParent();
        }

        surface->surface()->safeConnect(&WSurface::outputEntered,
                                        q,
                                        [handle](WOutput *output) {
                                            wlr_foreign_toplevel_handle_v1_output_enter(handle, output->handle());
                                        });

        surface->surface()->safeConnect(&WSurface::outputLeave,
                                        q,
                                        [handle](WOutput *output) {
                                            wlr_foreign_toplevel_handle_v1_output_leave(handle, output->handle());
                                        });

        data->requestActivateListener.connect(&handle->events.request_activate, [q, surface](wl_listener *, void *) {
            Q_EMIT q->requestActivate(surface);
        });

        data->requestMaximizeListener.connect(&handle->events.request_maximize, [q, surface](wl_listener *, void *data) {
            auto *event = static_cast<wlr_foreign_toplevel_handle_v1_maximized_event*>(data);
            Q_EMIT q->requestMaximize(surface, event->maximized);
        });

        data->requestMinimizeListener.connect(&handle->events.request_minimize, [q, surface](wl_listener *, void *data) {
            auto *event = static_cast<wlr_foreign_toplevel_handle_v1_minimized_event*>(data);
            Q_EMIT q->requestMinimize(surface, event->minimized);
        });

        data->requestFullscreenListener.connect(&handle->events.request_fullscreen, [q, surface](wl_listener *, void *data) {
            auto *event = static_cast<wlr_foreign_toplevel_handle_v1_fullscreen_event*>(data);
            Q_EMIT q->requestFullscreen(surface, event->fullscreen);
        });

        data->requestCloseListener.connect(&handle->events.request_close, [q, surface](wl_listener *, void *) {
            Q_EMIT q->requestClose(surface);
        });

        data->setRectangleListener.connect(&handle->events.set_rectangle, [q, surface](wl_listener *, void *data) {
            auto *event = static_cast<wlr_foreign_toplevel_handle_v1_set_rectangle_event*>(data);
            Q_EMIT q->rectangleChanged(surface, QRect{ event->x, event->y, event->width, event->height });
        });

        const auto title = surface->title().toUtf8();
        const auto appId = surface->appId().toLatin1();
        wlr_foreign_toplevel_handle_v1_set_title(handle, title);
        wlr_foreign_toplevel_handle_v1_set_app_id(handle, appId);
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

        auto *rawHandle = wlr_foreign_toplevel_handle_v1_create(q->handle());
        auto data = std::make_unique<HandleData>();
        data->handle.reset(rawHandle);
        initSurface(surface, data.get());
        surfaces.insert({ surface, std::move(data) });
    }

    void remove(WToplevelSurface *surface)
    {
        surfaces.erase(surface);
    }

    W_DECLARE_PUBLIC(WForeignToplevel)

    struct HandleDeleter {
        void operator()(wlr_foreign_toplevel_handle_v1 *p) const { if (p) wlr_foreign_toplevel_handle_v1_destroy(p); }
    };
    struct HandleData {
        std::unique_ptr<wlr_foreign_toplevel_handle_v1, HandleDeleter> handle;
        WScopedListener requestActivateListener;
        WScopedListener requestMaximizeListener;
        WScopedListener requestMinimizeListener;
        WScopedListener requestFullscreenListener;
        WScopedListener requestCloseListener;
        WScopedListener setRectangleListener;
    };
    std::map<WToplevelSurface *, std::unique_ptr<HandleData>> surfaces;
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

void WForeignToplevel::create(WServer *server)
{
    m_handle = wlr_foreign_toplevel_manager_v1_create(server->handle());
}

void WForeignToplevel::destroy([[maybe_unused]] WServer *server)
{
}

wl_global *WForeignToplevel::global() const
{
    return static_cast<wlr_foreign_toplevel_manager_v1*>(m_handle)->global;
}

WAYLIB_SERVER_END_NAMESPACE

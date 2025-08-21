// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wforeigntoplevelv1.h"

#include "private/wglobal_p.h"
#include "wglobal.h"
#include "woutput.h"
#include "wtoplevelsurface.h"
#include "wxdgtoplevelsurface.h"
#include "wxwaylandsurface.h"

#include <qwdisplay.h>
#include <qwforeigntoplevelhandlev1.h>
#include <qwxdgshell.h>

#include <map>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_LOGGING_CATEGORY(qLcWlrForeignToplevel, "waylib.protocols.foreigntoplevel", QtWarningMsg)

class Q_DECL_HIDDEN WForeignToplevelPrivate : public WObjectPrivate
{
public:
    WForeignToplevelPrivate(WForeignToplevel *qq)
        : WObjectPrivate(qq)
    {
    }

    void initSurface(WToplevelSurface *surface)
    {
        W_Q(WForeignToplevel);
        auto handle = surfaces.at(surface).get();
        surface->safeConnect(&WToplevelSurface::titleChanged, handle, [handle, surface] {
            const auto title = surface->title().toUtf8();
            handle->set_title(title);
        });

        surface->safeConnect(&WToplevelSurface::appIdChanged, handle, [handle, surface] {
            const auto appId = surface->appId().toLatin1();
            handle->set_app_id(appId);
        });

        surface->safeConnect(&WToplevelSurface::minimizeChanged, handle, [handle, surface] {
            handle->set_minimized(surface->isMinimized());
        });

        surface->safeConnect(&WToplevelSurface::maximizeChanged, handle, [handle, surface] {
            handle->set_maximized(surface->isMaximized());
        });

        surface->safeConnect(&WToplevelSurface::fullscreenChanged, handle, [handle, surface] {
            handle->set_fullscreen(surface->isFullScreen());
        });

        surface->safeConnect(&WToplevelSurface::activateChanged, handle, [handle, surface] {
            handle->set_activated(surface->isActivated());
        });

        if (auto *xdgSurface = qobject_cast<WXdgToplevelSurface *>(surface)) {
            auto updateSurfaceParent = [this, handle, xdgSurface] {
                WToplevelSurface *p = xdgSurface->parentXdgSurface();
                if (!p) {
                    handle->set_parent(nullptr);
                    return;
                }
                if (!surfaces.contains(p)) {
                    qCCritical(qLcWlrForeignToplevel)
                        << "Xdg toplevel surface " << xdgSurface
                        << "has set parent surface, but foreign_toplevel_handle for parent surface "
                           "not found!";
                    return;
                }
                handle->set_parent(*surfaces.at(p));
            };
            xdgSurface->safeConnect(&WXdgToplevelSurface::parentXdgSurfaceChanged,
                                    handle,
                                    updateSurfaceParent);
            updateSurfaceParent();
        } else if (auto *xwaylandSurface = qobject_cast<WXWaylandSurface *>(surface)) {
            auto updateSurfaceParent = [this, handle, xwaylandSurface] {
                WToplevelSurface *p = xwaylandSurface->parentXWaylandSurface();
                if (!p) {
                    handle->set_parent(nullptr);
                    return;
                }
                if (!surfaces.contains(p)) {
                    qCCritical(qLcWlrForeignToplevel)
                        << "X11 surface " << xwaylandSurface
                        << "has set parent surface, but foreign_toplevel_handle for parent surface "
                           "not found!";
                    return;
                }
                handle->set_parent(*surfaces.at(p));
            };
            xwaylandSurface->safeConnect(&WXWaylandSurface::parentXWaylandSurfaceChanged,
                                         handle,
                                         updateSurfaceParent);
            updateSurfaceParent();
        }

        surface->surface()->safeConnect(&WSurface::outputEntered,
                                        handle,
                                        [this, handle](WOutput *output) {
                                            handle->output_enter(output->nativeHandle());
                                        });

        surface->surface()->safeConnect(&WSurface::outputLeave,
                                        handle,
                                        [this, handle](WOutput *output) {
                                            handle->output_leave(output->nativeHandle());
                                        });

        QObject::connect(handle,
                         &qw_foreign_toplevel_handle_v1::notify_request_activate,
                         surface,
                         [surface, q]([[maybe_unused]] wlr_foreign_toplevel_handle_v1_activated_event *event) {
                             Q_EMIT q->requestActivate(surface);
                         });

        QObject::connect(handle,
                         &qw_foreign_toplevel_handle_v1::notify_request_maximize,
                         surface,
                         [surface, q](wlr_foreign_toplevel_handle_v1_maximized_event *event) {
                             Q_EMIT q->requestMaximize(surface, event->maximized);
                         });

        QObject::connect(handle,
                         &qw_foreign_toplevel_handle_v1::notify_request_minimize,
                         surface,
                         [surface, q](wlr_foreign_toplevel_handle_v1_minimized_event *event) {
                             Q_EMIT q->requestMinimize(surface, event->minimized);
                         });

        QObject::connect(handle,
                         &qw_foreign_toplevel_handle_v1::notify_request_fullscreen,
                         surface,
                         [surface, q](wlr_foreign_toplevel_handle_v1_fullscreen_event *event) {
                             Q_EMIT q->requestFullscreen(surface, event->fullscreen);
                         });

        QObject::connect(handle,
                         &qw_foreign_toplevel_handle_v1::notify_request_close,
                         surface,
                         [surface, q] {
                             Q_EMIT q->requestClose(surface);
                         });

        QObject::connect(handle,
                         &qw_foreign_toplevel_handle_v1::notify_set_rectangle,
                         surface,
                         [surface, q](wlr_foreign_toplevel_handle_v1_set_rectangle_event *event) {
                             Q_EMIT q->rectangleChanged(
                                 surface,
                                 QRect{ event->x, event->y, event->width, event->height });
                         });

        const auto title = surface->title().toUtf8();
        const auto appId = surface->appId().toLatin1();
        handle->set_title(title);
        handle->set_app_id(appId);
        handle->set_minimized(surface->isMinimized());
        handle->set_maximized(surface->isMaximized());
        handle->set_fullscreen(surface->isFullScreen());
        handle->set_activated(surface->isActivated());
    }

    void add(WToplevelSurface *surface)
    {
        W_Q(WForeignToplevel);

        if (surfaces.contains(surface)) {
            qCCritical(qLcWlrForeignToplevel)
                << surface << " has been add to foreign toplevel twice";
            return;
        }

        auto handle = qw_foreign_toplevel_handle_v1::create(
            *q->nativeInterface<qw_foreign_toplevel_manager_v1>());
        surfaces.insert({ surface, std::unique_ptr<qw_foreign_toplevel_handle_v1>(handle) });
        initSurface(surface);
    }

    void remove(WToplevelSurface *surface)
    {
        surfaces.erase(surface);
    }

    W_DECLARE_PUBLIC(WForeignToplevel)

    std::map<WToplevelSurface *, std::unique_ptr<qw_foreign_toplevel_handle_v1>> surfaces;
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
    W_D(WForeignToplevel);

    m_handle = qw_foreign_toplevel_manager_v1::create(*server->handle());
}

void WForeignToplevel::destroy([[maybe_unused]] WServer *server)
{
}

wl_global *WForeignToplevel::global() const
{
    return nativeInterface<qw_foreign_toplevel_manager_v1>()->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE

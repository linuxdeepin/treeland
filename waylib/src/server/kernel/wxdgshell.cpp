/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "wxdgshell.h"
#include "wsignalconnector.h"
#include "wxdgsurface.h"
#include "wsurfacelayout.h"
#include "wseat.h"

#include <qwseat.h>
#include <qwxdgshell.h>

#include <QPointer>

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgShellPrivate : public WObjectPrivate
{
public:
    WXdgShellPrivate(WXdgShell *qq, WSurfaceLayout *layout)
        : WObjectPrivate(qq)
        , layout(layout)
    {

    }

    // begin slot function
    void on_new_xdg_surface(wlr_xdg_surface *wlr_surface);
    void on_surface_destroy(QObject *data);

    void on_map(QWXdgSurface *xdgSurface);
    void on_unmap(QWXdgSurface *xdgSurface);
    // toplevel
    void on_request_move(wlr_xdg_toplevel_move_event *event);
    void on_request_resize(wlr_xdg_toplevel_resize_event *event);
    void on_request_maximize(QWXdgSurface *surface, bool maximize);
    // end slot function

    W_DECLARE_PUBLIC(WXdgShell)

    WServer *server = nullptr;
    QPointer<WSurfaceLayout> layout;
};

void WXdgShellPrivate::on_new_xdg_surface(wlr_xdg_surface *wlr_surface)
{
    // TODO: QWXdgSurface::from(wlr_surface)
    QWXdgSurface *xdgSurface = QWXdgSurface::from(wlr_surface->surface);
    auto surface = new WXdgSurface(reinterpret_cast<WXdgSurfaceHandle*>(xdgSurface));
    surface->moveToThread(server->thread());
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    QObject::connect(xdgSurface, &QObject::destroyed, server->slotOwner(), [this] (QObject *data) {
        on_surface_destroy(static_cast<QWXdgSurface*>(data));
    });

    QObject::connect(xdgSurface, &QWXdgSurface::map, server->slotOwner(), [this, xdgSurface] {
        on_map(xdgSurface);
    });
    QObject::connect(xdgSurface, &QWXdgSurface::unmap, server->slotOwner(), [this, xdgSurface] {
        on_unmap(xdgSurface);
    });

    if (auto toplevel = xdgSurface->topToplevel()) {
        QObject::connect(toplevel, &QWXdgToplevel::requestMove, server->slotOwner(), [this] (wlr_xdg_toplevel_move_event *event) {
            on_request_move(event);
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestResize, server->slotOwner(), [this] (wlr_xdg_toplevel_resize_event *event) {
            on_request_resize(event);
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestMaximize, server->slotOwner(), [this, xdgSurface] (bool maximize) {
            on_request_maximize(xdgSurface, maximize);
        });
    }

    layout->add(surface);
}

void WXdgShellPrivate::on_surface_destroy(QObject *data)
{
    QWXdgSurface *wlr_surface = qobject_cast<QWXdgSurface*>(data);
    auto surface = WXdgSurface::fromHandle(wlr_surface);
    Q_ASSERT(surface);

    if (layout)
        layout->remove(surface);
    surface->deleteLater();
}

void WXdgShellPrivate::on_map(QWXdgSurface *xdgSurface)
{
    Q_ASSERT(xdgSurface);
    auto surface = WXdgSurface::fromHandle<QWXdgSurface>(xdgSurface);
    layout->map(surface);
}

void WXdgShellPrivate::on_unmap(QWXdgSurface *xdgSurface)
{
    Q_ASSERT(xdgSurface);
    layout->unmap(WXdgSurface::fromHandle<QWXdgSurface>(xdgSurface));
}

void WXdgShellPrivate::on_request_move(wlr_xdg_toplevel_move_event *event)
{
    auto surface = WXdgSurface::fromHandle<QWXdgSurface>(QWXdgToplevel::from(event->toplevel));
    layout->requestMove(surface, WSeat::fromHandle<QWSeat>(QWSeat::from(event->seat->seat)), event->serial);
}

inline static Qt::Edges toQtEdge(uint32_t edges) {
    Qt::Edges qedges = Qt::Edges();

    if (edges & WLR_EDGE_TOP) {
        qedges |= Qt::TopEdge;
    }

    if (edges & WLR_EDGE_BOTTOM) {
        qedges |= Qt::BottomEdge;
    }

    if (edges & WLR_EDGE_LEFT) {
        qedges |= Qt::LeftEdge;
    }

    if (edges & WLR_EDGE_RIGHT) {
        qedges |= Qt::RightEdge;
    }

    return qedges;
}

void WXdgShellPrivate::on_request_resize(wlr_xdg_toplevel_resize_event *event)
{
    auto seat = WSeat::fromHandle<QWSeat>(QWSeat::from(event->seat->seat));
    auto surface = WXdgSurface::fromHandle<QWXdgSurface>(QWXdgToplevel::from(event->toplevel));
    layout->requestResize(surface, seat, toQtEdge(event->edges), event->serial);
}

void WXdgShellPrivate::on_request_maximize(QWXdgSurface *surface, bool maximize)
{
    if (maximize) {
        layout->requestMaximize(WXdgSurface::fromHandle<QWXdgSurface>(surface));
    } else {
        layout->requestUnmaximize(WXdgSurface::fromHandle<QWXdgSurface>(surface));
    }
}

WXdgShell::WXdgShell(WSurfaceLayout *layout)
    : WObject(*new WXdgShellPrivate(this, layout))
{

}

void WXdgShell::create(WServer *server)
{
    W_D(WXdgShell);
    d->server = server;
    // free follow display

    auto xdg_shell = QWXdgShell::create(server->nativeInterface<QWDisplay>(), 2);
    QObject::connect(xdg_shell, &QWXdgShell::newSurface, server->slotOwner(), [this] (wlr_xdg_surface *surface) {
        d_func()->on_new_xdg_surface(surface);
    });
}

void WXdgShell::destroy(WServer *server)
{
    W_D(WXdgShell);
    d->server = server;
}

WAYLIB_SERVER_END_NAMESPACE

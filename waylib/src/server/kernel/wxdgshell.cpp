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


extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#undef static
}

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
    void on_new_xdg_surface(void *data);
    void on_surface_destroy(void *data);

    void on_map(void *data);
    void on_unmap(void *data);
    // toplevel
    void on_request_move(void *data);
    void on_request_resize(void *data);
    void on_request_maximize(void *data);
    // end slot function

    W_DECLARE_PUBLIC(WXdgShell)

    WServer *server = nullptr;
    WSurfaceLayout *layout = nullptr;
    WSignalConnector sc;
};

void WXdgShellPrivate::on_new_xdg_surface(void *data)
{
    wlr_xdg_surface *wlr_surface = reinterpret_cast<struct wlr_xdg_surface*>(data);
    auto surface = new WXdgSurface(reinterpret_cast<WXdgSurfaceHandle*>(wlr_surface));
    surface->moveToThread(server->thread());
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    sc.connect(&wlr_surface->events.destroy, this, &WXdgShellPrivate::on_surface_destroy);

    sc.connect(&wlr_surface->events.map,
               this, &WXdgShellPrivate::on_map);
    sc.connect(&wlr_surface->events.unmap,
               this, &WXdgShellPrivate::on_unmap);

    if (wlr_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        auto toplevel = wlr_surface->toplevel;
        sc.connect(&toplevel->events.request_move,
                   this, &WXdgShellPrivate::on_request_move);
        sc.connect(&toplevel->events.request_resize,
                   this, &WXdgShellPrivate::on_request_resize);
        sc.connect(&toplevel->events.request_maximize,
                   this, &WXdgShellPrivate::on_request_maximize);
    }

    layout->add(surface);
}

void WXdgShellPrivate::on_surface_destroy(void *data)
{
    wlr_xdg_surface *wlr_surface = reinterpret_cast<struct wlr_xdg_surface*>(data);
    auto surface = WXdgSurface::fromHandle(wlr_surface);
    Q_ASSERT(surface);

    layout->remove(surface);
    surface->deleteLater();

    // clear the signal connection
    sc.invalidate(&wlr_surface->events.destroy);
    sc.invalidate(&wlr_surface->events.map);
    sc.invalidate(&wlr_surface->events.unmap);

    if (wlr_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        auto toplevel = wlr_surface->toplevel;
        sc.invalidate(&toplevel->events.request_move);
        sc.invalidate(&toplevel->events.request_resize);
        sc.invalidate(&toplevel->events.request_maximize);
    }
}

void WXdgShellPrivate::on_map(void *data)
{
    wlr_xdg_surface *wlr_surface = reinterpret_cast<struct wlr_xdg_surface*>(data);
    auto surface = WXdgSurface::fromHandle<wlr_xdg_surface>(wlr_surface);
    layout->map(surface);

    if (wlr_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        double sx = 0, sy = 0;
        wlr_xdg_popup_get_position(wlr_surface->popup, &sx, &sy);
        layout->setPosition(surface, surface->positionToGlobal(QPointF(sx, sy)));
    }
}

void WXdgShellPrivate::on_unmap(void *data)
{
    wlr_xdg_surface *wlr_surface = reinterpret_cast<struct wlr_xdg_surface*>(data);
    layout->unmap(WXdgSurface::fromHandle<wlr_xdg_surface>(wlr_surface));
}

void WXdgShellPrivate::on_request_move(void *data)
{
    auto event = reinterpret_cast<wlr_xdg_toplevel_move_event*>(data);
    auto surface = WXdgSurface::fromHandle<wlr_xdg_surface>(event->surface);
    layout->requestMove(surface, WSeat::fromHandle<wlr_seat>(event->seat->seat), event->serial);
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

void WXdgShellPrivate::on_request_resize(void *data)
{
    auto event = reinterpret_cast<wlr_xdg_toplevel_resize_event*>(data);
    auto seat = WSeat::fromHandle<wlr_seat>(event->seat->seat);
    auto surface = WXdgSurface::fromHandle<wlr_xdg_surface>(event->surface);
    layout->requestResize(surface, seat, toQtEdge(event->edges), event->serial);
}

void WXdgShellPrivate::on_request_maximize(void *data)
{
    auto surface = static_cast<wlr_xdg_surface*>(data);
    if (surface->toplevel->client_pending.maximized) {
        layout->requestMaximize(WXdgSurface::fromHandle<wlr_xdg_surface>(surface));
    } else {
        layout->requestUnmaximize(WXdgSurface::fromHandle<wlr_xdg_surface>(surface));
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
    auto xdg_shell = wlr_xdg_shell_create(server->nativeInterface<wl_display>());
    d->sc.connect(&xdg_shell->events.new_surface, d, &WXdgShellPrivate::on_new_xdg_surface);
}

void WXdgShell::destroy(WServer *server)
{
    W_D(WXdgShell);
    d->server = server;
}

WAYLIB_SERVER_END_NAMESPACE

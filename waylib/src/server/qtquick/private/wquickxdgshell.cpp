// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxdgshell_p.h"
#include "wseat.h"
#include "woutput.h"
#include "wxdgshell.h"
#include "wxdgsurface.h"
#include "wsurface.h"

#include <qwxdgshell.h>
#include <qwseat.h>

#include <QCoreApplication>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
#include <wlr/util/edges.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class XdgShell : public WXdgShell
{
public:
    XdgShell(WQuickXdgShell *qq)
        : qq(qq) {}

    void surfaceAdded(WXdgSurface *surface) override;
    void surfaceRemoved(WXdgSurface *surface) override;

    WQuickXdgShell *qq;
};

class WQuickXdgShellPrivate : public WObjectPrivate, public WXdgShell
{
public:
    WQuickXdgShellPrivate(WQuickXdgShell *qq)
        : WObjectPrivate(qq)
        , WXdgShell()
    {
    }

    W_DECLARE_PUBLIC(WQuickXdgShell)

    XdgShell *xdgShell = nullptr;
};

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

void XdgShell::surfaceAdded(WXdgSurface *surface)
{
    if (auto toplevel = surface->handle()->topToplevel()) {
        QObject::connect(toplevel, &QWXdgToplevel::requestMove, qq, [this] (wlr_xdg_toplevel_move_event *event) {
            auto surface = WXdgSurface::fromHandle(QWXdgToplevel::from(event->toplevel));
            auto seat = WSeat::fromHandle(QWSeat::from(event->seat->seat));
            Q_EMIT qq->requestMove(surface, seat, event->serial);
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestResize, qq, [this] (wlr_xdg_toplevel_resize_event *event) {
            auto surface = WXdgSurface::fromHandle(QWXdgToplevel::from(event->toplevel));
            auto seat = WSeat::fromHandle(QWSeat::from(event->seat->seat));
            Q_EMIT qq->requestResize(surface, seat, toQtEdge(event->edges), event->serial);
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestMaximize, qq, [this, surface] (bool maximize) {
            if (maximize) {
                Q_EMIT qq->requestMaximize(surface);
            } else {
                Q_EMIT qq->requestToNormalState(surface);
            }
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestFullscreen, qq, [this, surface] (bool fullscreen) {
            if (fullscreen) {
                Q_EMIT qq->requestFullscreen(surface);
            } else {
                Q_EMIT qq->requestToNormalState(surface);
            }
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestShowWindowMenu, qq, [this] (wlr_xdg_toplevel_show_window_menu_event *event) {
            auto surface = WXdgSurface::fromHandle(QWXdgToplevel::from(event->toplevel));
            auto seat = WSeat::fromHandle(QWSeat::from(event->seat->seat));
            Q_EMIT qq->requestShowWindowMenu(surface, seat, QPoint(event->x, event->y), event->serial);
        });
    }

    Q_EMIT qq->surfaceAdded(surface);
}

void XdgShell::surfaceRemoved(WXdgSurface *surface)
{
    surface->disconnect(surface, nullptr, qq, nullptr);

    Q_EMIT qq->surfaceRemoved(surface);
}

WQuickXdgShell::WQuickXdgShell(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickXdgShellPrivate(this), nullptr)
{

}

void WQuickXdgShell::create()
{
    W_D(WQuickXdgShell);

    d->xdgShell = server()->attach<XdgShell>(this);
    WQuickWaylandServerInterface::polish();
}

WAYLIB_SERVER_END_NAMESPACE

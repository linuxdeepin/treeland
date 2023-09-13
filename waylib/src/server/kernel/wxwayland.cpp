// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwayland.h"
#include "wxwaylandsurface.h"
#include "wseat.h"

#include <qwseat.h>
#include <qwxwayland.h>
#include <qwxwaylandsurface.h>
#include <qwxwaylandshellv1.h>

extern "C" {
#define class className
#include <wlr/xwayland.h>
#undef class
}

#include <xcb/xcb.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WXWaylandPrivate : public WObjectPrivate
{
public:
    WXWaylandPrivate(WXWayland *qq, QWCompositor *compositor, bool lazy)
        : WObjectPrivate(qq)
        , compositor(compositor)
        , lazy(lazy)
    {

    }
    ~WXWaylandPrivate() {
        if (xcbConnection)
            xcb_disconnect(xcbConnection);
    }

    void init();

    // begin slot function
    void on_new_surface(wlr_xwayland_surface *xwl_surface);
    void on_surface_destroy(QWXWaylandSurface *xwl_surface);
    // end slot function

    W_DECLARE_PUBLIC(WXWayland)

    QWCompositor *compositor;
    bool lazy = true;
    QVector<WXWaylandSurface*> surfaceList;
    xcb_connection_t *xcbConnection = nullptr;
    QVector<xcb_atom_t> atoms;
};

static const QByteArrayView atom_map[WXWayland::AtomCount] = {
    "", // None
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_WINDOW_TYPE_UTILITY",
    "_NET_WM_WINDOW_TYPE_TOOLTIP",
    "_NET_WM_WINDOW_TYPE_DND",
    "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
    "_NET_WM_WINDOW_TYPE_POPUP_MENU",
    "_NET_WM_WINDOW_TYPE_COMBO",
    "_NET_WM_WINDOW_TYPE_MENU",
    "_NET_WM_WINDOW_TYPE_NOTIFICATION",
    "_NET_WM_WINDOW_TYPE_SPLASH",
};

void WXWaylandPrivate::init()
{
    W_Q(WXWayland);
    xcbConnection = xcb_connect(q->displayName().constData(), nullptr);
    int err = xcb_connection_has_error(xcbConnection);
    if (err != 0) {
        qFatal("Can't connect to XWayland by xcb_connect");
        return;
    }
    if (!xcbConnection)
        return;

    xcb_intern_atom_cookie_t cookies[WXWayland::AtomCount];
    for (int i = WXWayland::AtomNone + 1; i < WXWayland::AtomCount; ++i) {
        cookies[i] = xcb_intern_atom(xcbConnection, 0,
                                     atom_map[i].length(),
                                     atom_map[i].constData());
    }

    atoms.resize(WXWayland::AtomCount);
    for (int i = WXWayland::AtomNone + 1; i < WXWayland::AtomCount; ++i) {
        xcb_generic_error_t *error;
        xcb_intern_atom_reply_t *reply =
            xcb_intern_atom_reply(xcbConnection, cookies[i], &error);
        if (reply && !error)
            atoms[i] = reply->atom;
        free(reply);

        if (error) {
            atoms[i] = XCB_ATOM_NONE;
            free(error);
            continue;
        }
    }
}

void WXWaylandPrivate::on_new_surface(wlr_xwayland_surface *xwl_surface)
{
    W_Q(WXWayland);

    auto server = q->server();
    QWXWaylandSurface *xwlSurface = QWXWaylandSurface::from(xwl_surface);
    auto surface = new WXWaylandSurface(xwlSurface, q, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    QObject::connect(xwlSurface, &QWXWaylandSurface::beforeDestroy,
                     server->slotOwner(), [this] (QWXWaylandSurface *surface) {
        on_surface_destroy(surface);
    });

    surfaceList.append(surface);
    q->surfaceAdded(surface);
}

void WXWaylandPrivate::on_surface_destroy(QWXWaylandSurface *xwl_surface)
{
    auto surface = WXWaylandSurface::fromHandle(xwl_surface);
    Q_ASSERT(surface);
    bool ok = surfaceList.removeOne(surface);
    Q_ASSERT(ok);
    q_func()->surfaceRemoved(surface);
    surface->deleteLater();
}

WXWayland::WXWayland(QWCompositor *compositor, bool lazy)
    : WObject(*new WXWaylandPrivate(this, compositor, lazy))
{

}

QByteArray WXWayland::displayName() const
{
    W_DC(WXWayland);
    return isValid() ? QByteArray(std::as_const(handle()->handle()->display_name)) : QByteArray();
}

xcb_atom_t WXWayland::atom(XcbAtom type) const
{
    W_DC(WXWayland);
    return d->atoms.at(type);
}

WXWayland::XcbAtom WXWayland::atomType(xcb_atom_t atom) const
{
    W_DC(WXWayland);
    for (int i = AtomNone; i < AtomCount; ++i) {
        if (d->atoms.at(i) == atom)
            return static_cast<XcbAtom>(i);
    }

    return AtomNone;
}

void WXWayland::setSeat(WSeat *seat)
{
    W_D(WXWayland);
    if (auto handle = this->handle())
        handle->setSeat(seat->handle());
}

WSeat *WXWayland::seat() const
{
    W_DC(WXWayland);
    if (!handle())
        return nullptr;
    if (!handle()->handle()->seat)
        return nullptr;
    auto seat = QWSeat::from(handle()->handle()->seat);
    return WSeat::fromHandle(seat);
}

xcb_connection_t *WXWayland::xcbConnection() const
{
    W_DC(WXWayland);
    return d->xcbConnection;
}

QVector<WXWaylandSurface*> WXWayland::surfaceList() const
{
    W_DC(WXWayland);
    return d->surfaceList;
}

void WXWayland::surfaceAdded(WXWaylandSurface *)
{

}

void WXWayland::surfaceRemoved(WXWaylandSurface *)
{

}

void WXWayland::create(WServer *server)
{
    W_D(WXWayland);
    // free follow display

    auto handle = QWXWayland::create(server->handle(), d->compositor, d->lazy);
    QObject::connect(handle, &QWXWayland::newSurface, server->slotOwner(), [d] (wlr_xwayland_surface *surface) {
        d->on_new_surface(surface);
    });
    QObject::connect(handle, &QWXWayland::ready, server->slotOwner(), [d] {
        d->init();
    });

    m_handle = handle;
}

void WXWayland::destroy(WServer *server)
{
    Q_UNUSED(server);
    W_D(WXWayland);

    auto list = d->surfaceList;
    d->surfaceList.clear();

    for (auto surface : list) {
        surfaceRemoved(surface);
        surface->deleteLater();
    }
}

WAYLIB_SERVER_END_NAMESPACE

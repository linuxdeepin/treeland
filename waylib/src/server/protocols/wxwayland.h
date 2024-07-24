// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

QW_BEGIN_NAMESPACE
class qw_xwayland;
class qw_compositor;
QW_END_NAMESPACE

struct xcb_connection_t;
typedef uint32_t xcb_atom_t;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WXWaylandSurface;
class WXWaylandPrivate;
class WAYLIB_SERVER_EXPORT WXWayland : public WWrapObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXWayland)
public:
    enum XcbAtom {
        AtomNone = 0,
        NET_WM_WINDOW_TYPE_NORMAL,
        NET_WM_WINDOW_TYPE_UTILITY,
        NET_WM_WINDOW_TYPE_TOOLTIP,
        NET_WM_WINDOW_TYPE_DND,
        NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
        NET_WM_WINDOW_TYPE_POPUP_MENU,
        NET_WM_WINDOW_TYPE_COMBO,
        NET_WM_WINDOW_TYPE_MENU,
        NET_WM_WINDOW_TYPE_NOTIFICATION,
        NET_WM_WINDOW_TYPE_SPLASH,
        AtomCount
    };

    WXWayland(QW_NAMESPACE::qw_compositor *compositor, bool lazy = true);

    inline QW_NAMESPACE::qw_xwayland *handle() const {
        return nativeInterface<QW_NAMESPACE::qw_xwayland>();
    }

    QByteArray displayName() const;

    xcb_atom_t atom(XcbAtom type) const;
    XcbAtom atomType(xcb_atom_t atom) const;

    void setSeat(WSeat *seat);
    WSeat *seat() const;

    xcb_connection_t *xcbConnection() const;
    QVector<WXWaylandSurface*> surfaceList() const;

    WSocket *ownsSocket() const;
    void setOwnsSocket(WSocket *socket);

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void ready();
    void surfaceAdded(WXWaylandSurface *surface);
    void surfaceRemoved(WXWaylandSurface *surface);
    void toplevelAdded(WXWaylandSurface *surface);
    void toplevelRemoved(WXWaylandSurface *surface);

protected:
    void addSurface(WXWaylandSurface *surface);
    void removeSurface(WXWaylandSurface *surface);
    void addToplevel(WXWaylandSurface *surface);
    void removeToplevel(WXWaylandSurface *surface);
    void onIsToplevelChanged();

    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

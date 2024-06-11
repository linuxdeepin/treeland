// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

QW_BEGIN_NAMESPACE
class QWXWayland;
class QWCompositor;
QW_END_NAMESPACE

struct xcb_connection_t;
typedef uint32_t xcb_atom_t;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WXWaylandSurface;
class WXWaylandPrivate;
class WAYLIB_SERVER_EXPORT WXWayland : public WServerInterface, public WWrapObject
{
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

    WXWayland(QW_NAMESPACE::QWCompositor *compositor, bool lazy = true);

    inline QW_NAMESPACE::QWXWayland *handle() const {
        return nativeInterface<QW_NAMESPACE::QWXWayland>();
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

protected:
    virtual void surfaceAdded(WXWaylandSurface *surface);
    virtual void surfaceRemoved(WXWaylandSurface *surface);

    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

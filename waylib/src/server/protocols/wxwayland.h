// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <xcb/xcb.h>

#include <WServer>

#include <QByteArray>
#include <QMap>
#include <QVector>

#include <functional>

Q_MOC_INCLUDE("wxwaylandsurface.h")

QW_BEGIN_NAMESPACE
class qw_xwayland;
class qw_compositor;
QW_END_NAMESPACE

struct wlr_xwayland;
struct xcb_screen_t;

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
        _NET_WM_WINDOW_TYPE_NORMAL,
        _NET_WM_WINDOW_TYPE_UTILITY,
        _NET_WM_WINDOW_TYPE_TOOLTIP,
        _NET_WM_WINDOW_TYPE_DND,
        _NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
        _NET_WM_WINDOW_TYPE_POPUP_MENU,
        _NET_WM_WINDOW_TYPE_COMBO,
        _NET_WM_WINDOW_TYPE_MENU,
        _NET_WM_WINDOW_TYPE_NOTIFICATION,
        _NET_WM_WINDOW_TYPE_SPLASH,
        _NET_WM_WINDOW_TYPE_DIALOG,
        _NET_SUPPORTED,
        AtomCount
    };
    Q_ENUM(XcbAtom)

    static WXWayland *fromHandle(wlr_xwayland *handle);

    WXWayland(QW_NAMESPACE::qw_compositor *compositor, bool lazy = true);

    inline QW_NAMESPACE::qw_xwayland *handle() const {
        return nativeInterface<QW_NAMESPACE::qw_xwayland>();
    }

    QByteArray displayName() const;

    xcb_atom_t atom(XcbAtom type) const;
    xcb_atom_t atom(const QByteArray &name) const;
    XcbAtom atomType(xcb_atom_t atom) const;
    QVarLengthArray<xcb_atom_t> supportedAtoms() const;
    void setSupportedAtoms(const QVarLengthArray<xcb_atom_t> &atoms);
    void setAtomSupported(xcb_atom_t atom, bool supported);

    void setSeat(WSeat *seat);
    WSeat *seat() const;

    xcb_connection_t *xcbConnection() const;
    xcb_screen_t *xcbScreen() const;

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
    void windowPropertyChanged(WXWaylandSurface *surface, xcb_atom_t atom);

protected:
    void addSurface(WXWaylandSurface *surface);
    void removeSurface(WXWaylandSurface *surface);
    void addToplevel(WXWaylandSurface *surface);
    void removeToplevel(WXWaylandSurface *surface);
    void onIsToplevelChanged();

    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

protected:
    bool event(QEvent *ev) override;

public:
    struct AsyncPropRequest
    {
        xcb_atom_t atom = XCB_ATOM_NONE;
        xcb_atom_t type = XCB_ATOM_ANY;
    };

    // Read properties for windowId asynchronously.
    // - Fires callback(result) when all replies arrive or timeoutMs elapses.
    // - If a specific property was never requested, it won't be in the result map.
    void readAsyncProperties(
        xcb_window_t windowId,
        const QVector<AsyncPropRequest> &requests,
        int timeoutMs,
        std::function<void(xcb_window_t, const QMap<xcb_atom_t, QByteArray> &)> callback);

    // Cancel pending async property read for windowId.
    // Call this when the window/surface is being destroyed.
    void cancelAsyncProperties(xcb_window_t windowId);

private:
    friend bool xwayland_user_event_handler(wlr_xwayland *, xcb_generic_event_t *);
};

WAYLIB_SERVER_END_NAMESPACE

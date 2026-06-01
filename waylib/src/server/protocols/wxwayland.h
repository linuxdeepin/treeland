// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <xcb/xcb.h>

#include <WServer>

#include <QByteArray>

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

namespace Xcb {

class Property
{
public:
    Property(xcb_connection_t *conn,
             xcb_window_t win,
             xcb_atom_t prop,
             xcb_atom_t type,
             uint32_t offset = 0,
             uint32_t length = 1024);
    Property();
    ~Property();
    Property(const Property &) = delete;
    Property &operator=(const Property &) = delete;
    Property(Property &&other) noexcept;
    Property &operator=(Property &&other) noexcept;

    QByteArray toByteArray() const;

private:
    void getReply() const;

    xcb_connection_t *m_conn = nullptr;
    xcb_window_t m_win = XCB_WINDOW_NONE;
    xcb_atom_t m_type = XCB_ATOM_NONE;
    uint32_t m_offset = 0;
    uint32_t m_length = 0;
    mutable bool m_retrieved = false;
    mutable xcb_get_property_cookie_t m_cookie = {};
    mutable xcb_get_property_reply_t *m_reply = nullptr;
};

} // namespace Xcb

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
};

WAYLIB_SERVER_END_NAMESPACE

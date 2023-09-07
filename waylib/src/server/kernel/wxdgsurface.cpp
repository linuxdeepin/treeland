// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgsurface.h"
#include "private/wsurface_p.h"
#include "wseat.h"
#include "wtools.h"

#include <qwxdgshell.h>
#include <qwseat.h>
#include <qwcompositor.h>

#include <QDebug>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgSurfacePrivate : public WObjectPrivate {
public:
    WXdgSurfacePrivate(WXdgSurface *qq, QWXdgSurface *handle);
    ~WXdgSurfacePrivate();

    inline wlr_xdg_surface *nativeHandle() const {
        Q_ASSERT(handle);
        return handle->handle();
    }

    wl_client *waylandClient() const {
        return nativeHandle()->client->client;
    }

    // begin slot function
    void on_configure(wlr_xdg_surface_configure *event);
    void on_ack_configure(wlr_xdg_surface_configure *event);
    // end slot function

    void init();
    void connect();
    void updatePosition();

    W_DECLARE_PUBLIC(WXdgSurface)

    QPointer<QWXdgSurface> handle;
    WSurface *surface = nullptr;
    QPointF position;
    uint resizeing:1;
    uint activated:1;
    uint maximized:1;
    uint minimized:1;
};

WXdgSurfacePrivate::WXdgSurfacePrivate(WXdgSurface *qq, QWXdgSurface *hh)
    : WObjectPrivate(qq)
    , handle(hh)
    , resizeing(false)
    , activated(false)
    , maximized(false)
    , minimized(false)
{
}

WXdgSurfacePrivate::~WXdgSurfacePrivate()
{
    if (handle)
        handle->setData(this, nullptr);
    surface->removeAttachedData<WXdgSurface>();
}

void WXdgSurfacePrivate::on_configure(wlr_xdg_surface_configure *event)
{
    if (handle->topToplevel()) {
        W_Q(WXdgSurface);

        if (event->toplevel_configure->resizing != resizeing) {
            resizeing = event->toplevel_configure->resizing;
            Q_EMIT q->resizeingChanged();
        }

        if (event->toplevel_configure->activated != activated) {
            activated = event->toplevel_configure->activated;
            Q_EMIT q->activateChanged();
        }

        if (event->toplevel_configure->maximized != maximized) {
            maximized = event->toplevel_configure->maximized;
            Q_EMIT q->maximizeChanged();
        }
    }
}

void WXdgSurfacePrivate::on_ack_configure(wlr_xdg_surface_configure *event)
{
    Q_UNUSED(event)
//    auto config = reinterpret_cast<wlr_xdg_surface_configure*>(data);
}

void WXdgSurfacePrivate::init()
{
    W_Q(WXdgSurface);
    handle->setData(this, q);

    Q_ASSERT(!q->surface());
    surface = new WSurface(handle->surface(), q);
    surface->setAttachedData<WXdgSurface>(q);

    connect();
}

void WXdgSurfacePrivate::connect()
{
    W_Q(WXdgSurface);

    QObject::connect(handle, &QWXdgSurface::configure, q, [this] (wlr_xdg_surface_configure *event) {
        on_configure(event);
    });
    QObject::connect(handle, &QWXdgSurface::ackConfigure, q, [this] (wlr_xdg_surface_configure *event) {
        on_ack_configure(event);
    });

    if (auto toplevel = handle->topToplevel()) {
        QObject::connect(toplevel, &QWXdgToplevel::requestMove, q, [q] (wlr_xdg_toplevel_move_event *event) {
            auto seat = WSeat::fromHandle(QWSeat::from(event->seat->seat));
            Q_EMIT q->requestMove(seat, event->serial);
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestResize, q, [q] (wlr_xdg_toplevel_resize_event *event) {
            auto seat = WSeat::fromHandle(QWSeat::from(event->seat->seat));
            Q_EMIT q->requestResize(seat, WTools::toQtEdge(event->edges), event->serial);
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestMaximize, q, [q] (bool maximize) {
            if (maximize) {
                Q_EMIT q->requestMaximize();
            } else {
                Q_EMIT q->requestCancelMaximize();
            }
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestMinimize, q, [q] (bool minimize) {
            if (minimize) {
                Q_EMIT q->requestMinimize();
            } else {
                Q_EMIT q->requestCancelMinimize();
            }
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestFullscreen, q, [q] (bool fullscreen) {
            // TODO: implement fullscreen support
            if (fullscreen) {
                Q_EMIT q->requestFullscreen();
            } else {
                Q_EMIT q->requestCancelFullscreen();
            }
        });
        QObject::connect(toplevel, &QWXdgToplevel::requestShowWindowMenu, q, [q] (wlr_xdg_toplevel_show_window_menu_event *event) {
            auto seat = WSeat::fromHandle(QWSeat::from(event->seat->seat));
            Q_EMIT q->requestShowWindowMenu(seat, QPoint(event->x, event->y), event->serial);
        });

        QObject::connect(toplevel, &QWXdgToplevel::parentChanged, q, &WXdgSurface::parentXdgSurfaceChanged);
    }
}

WXdgSurface::WXdgSurface(QWXdgSurface *handle, QObject *parent)
    : WToplevelSurface(parent)
    , WObject(*new WXdgSurfacePrivate(this, handle))
{
    d_func()->init();
}

WXdgSurface::~WXdgSurface()
{

}

bool WXdgSurface::isPopup() const
{
    W_DC(WXdgSurface);
    return d->nativeHandle()->role == WLR_XDG_SURFACE_ROLE_POPUP;
}

bool WXdgSurface::doesNotAcceptFocus() const
{
    W_DC(WXdgSurface);
    return d->nativeHandle()->role == WLR_XDG_SURFACE_ROLE_NONE;
}

WSurface *WXdgSurface::surface() const
{
    W_D(const WXdgSurface);
    return d->surface;
}

QWXdgSurface *WXdgSurface::handle() const
{
    W_DC(WXdgSurface);
    return d->handle;
}

QWSurface *WXdgSurface::inputTargetAt(QPointF &localPos) const
{
    W_DC(WXdgSurface);
    // find a wlr_suface object who can receive the events
    const QPointF pos = localPos;
    auto sur = d->handle->surfaceAt(pos, &localPos);

    return sur;
}

WXdgSurface *WXdgSurface::fromHandle(QWXdgSurface *handle)
{
    return handle->getData<WXdgSurface>();
}

WXdgSurface *WXdgSurface::fromSurface(WSurface *surface)
{
    return surface->getAttachedData<WXdgSurface>();
}

void WXdgSurface::resize(const QSize &size)
{
    W_D(WXdgSurface);

    if (auto toplevel = d->handle->topToplevel()) {
        toplevel->setSize(size);
    }
}

bool WXdgSurface::isResizeing() const
{
    W_DC(WXdgSurface);
    return d->resizeing;
}

bool WXdgSurface::isActivated() const
{
    W_DC(WXdgSurface);
    return d->activated;
}

bool WXdgSurface::isMaximized() const
{
    W_DC(WXdgSurface);
    return d->maximized;
}

bool WXdgSurface::isMinimized() const
{
    W_DC(WXdgSurface);
    return d->minimized;
}

QRect WXdgSurface::getContentGeometry() const
{
    W_DC(WXdgSurface);
    return d->handle->getGeometry();
}

QSize WXdgSurface::minSize() const
{
    W_DC(WXdgSurface);
    if (auto toplevel = d->handle->topToplevel())
        return QSize(toplevel->handle()->current.min_width,
                     toplevel->handle()->current.min_height);

    return QSize();
}

QSize WXdgSurface::maxSize() const
{
    W_DC(WXdgSurface);
    if (auto toplevel = d->handle->topToplevel())
        return QSize(toplevel->handle()->current.max_width,
                     toplevel->handle()->current.max_height);

    return QSize();
}

WXdgSurface *WXdgSurface::parentXdgSurface() const
{
    W_DC(WXdgSurface);

    if (auto toplevel = d->handle->topToplevel()) {
        auto parent = toplevel->handle()->parent;
        if (!parent)
            return nullptr;
        return fromHandle(QWXdgToplevel::from(parent));
    } else if (auto popup = d->handle->toPopup()) {
        auto parent = popup->handle()->parent;
        if (!parent)
            return nullptr;
        auto xdgParent = QWXdgSurface::from(QWSurface::from(parent));
        Q_ASSERT(xdgParent);
        return fromHandle(xdgParent);
    }

    return nullptr;
}

void WXdgSurface::setResizeing(bool resizeing)
{
    W_D(WXdgSurface);
    if (auto toplevel = d->handle->topToplevel()) {
        toplevel->setResizing(resizeing);
    }
}

void WXdgSurface::setMaximize(bool on)
{
    W_D(WXdgSurface);
    if (auto toplevel = d->handle->topToplevel()) {
        toplevel->setMaximized(on);
    }
}

void WXdgSurface::setMinimize(bool on)
{
    W_D(WXdgSurface);

    if (d->minimized != on) {
        d->minimized = on;
        Q_EMIT minimizeChanged();
    }
}

void WXdgSurface::setActivate(bool on)
{
    W_D(WXdgSurface);
    if (auto toplevel = d->handle->topToplevel()) {
        toplevel->setActivated(on);
    }
}

bool WXdgSurface::checkNewSize(const QSize &size)
{
    W_D(WXdgSurface);
    if (auto toplevel = d->handle->topToplevel()) {
        if (size.width() > toplevel->handle()->current.max_width
            && toplevel->handle()->current.max_width > 0)
            return false;
        if (size.height() > toplevel->handle()->current.max_height
            && toplevel->handle()->current.max_height > 0)
            return false;
        if (size.width() < toplevel->handle()->current.min_width
            && toplevel->handle()->current.min_width> 0)
            return false;
        if (size.height() < toplevel->handle()->current.min_height
            && toplevel->handle()->current.min_height > 0)
            return false;
        return true;
    }

    return false;
}

WAYLIB_SERVER_END_NAMESPACE

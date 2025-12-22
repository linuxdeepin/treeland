// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgtoplevelsurface.h"
#include "private/wtoplevelsurface_p.h"
#include "wseat.h"
#include "wtools.h"

#include <qwxdgshell.h>
#include <qwcompositor.h>
#include <qwseat.h>
#include <qwbox.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgToplevelSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WXdgToplevelSurfacePrivate(WXdgToplevelSurface *qq, qw_xdg_toplevel *handle);
    ~WXdgToplevelSurfacePrivate();

    WWRAP_HANDLE_FUNCTIONS(qw_xdg_toplevel, wlr_xdg_toplevel)

    wl_client *waylandClient() const override {
        return nativeHandle()->base->client->client;
    }

    // begin slot function
    void on_configure(wlr_xdg_surface_configure *event);
    void on_ack_configure(wlr_xdg_surface_configure *event);
    // end slot function

    void init();
    void connect();

    void instantRelease() override;

    W_DECLARE_PUBLIC(WXdgToplevelSurface)

    WSurface *surface = nullptr;
    QPointF position;
    uint resizeing:1;
    uint activated:1;
    uint maximized:1;
    uint minimized:1;
    uint fullscreen:1;
};

WXdgToplevelSurfacePrivate::WXdgToplevelSurfacePrivate(WXdgToplevelSurface *qq, qw_xdg_toplevel *hh)
    : WToplevelSurfacePrivate(qq)
    , resizeing(false)
    , activated(false)
    , maximized(false)
    , minimized(false)
    , fullscreen(false)
{
    initHandle(hh);
}

WXdgToplevelSurfacePrivate::~WXdgToplevelSurfacePrivate()
{

}

void WXdgToplevelSurfacePrivate::instantRelease()
{
    if (!surface)
        return;
    W_Q(WXdgToplevelSurface);
    handle()->set_data(nullptr, nullptr);

    auto xdgSurface = qw_xdg_surface::from(nativeHandle()->base);
    xdgSurface->disconnect(q);
    handle()->disconnect(q);
    surface->safeDeleteLater();
    surface = nullptr;
}

void WXdgToplevelSurfacePrivate::on_configure(wlr_xdg_surface_configure *event)
{
    W_Q(WXdgToplevelSurface);

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

    if (event->toplevel_configure->fullscreen != fullscreen) {
        fullscreen = event->toplevel_configure->fullscreen;
        Q_EMIT q->fullscreenChanged();
    }
}

void WXdgToplevelSurfacePrivate::init()
{
    W_Q(WXdgToplevelSurface);
    handle()->set_data(this, q);

    Q_ASSERT(!q->surface());
    surface = new WSurface(qw_surface::from(nativeHandle()->base->surface), q);
    surface->setAttachedData<WXdgToplevelSurface>(q);

    connect();
}

void WXdgToplevelSurfacePrivate::connect()
{
    W_Q(WXdgToplevelSurface);

    auto surface = qw_xdg_surface::from(nativeHandle()->base);
    QObject::connect(surface, &qw_xdg_surface::notify_configure, q, [this] (wlr_xdg_surface_configure *event) {
        on_configure(event);
    });

    // TODO: use safeConnect for toplevel
    QObject::connect(handle(), &qw_xdg_toplevel::notify_request_move, q, [q] (wlr_xdg_toplevel_move_event *event) {
        auto seat = WSeat::fromHandle(qw_seat::from(event->seat->seat));
        Q_EMIT q->requestMove(seat, event->serial);
    });
    QObject::connect(handle(), &qw_xdg_toplevel::notify_request_resize, q, [q] (wlr_xdg_toplevel_resize_event *event) {
        auto seat = WSeat::fromHandle(qw_seat::from(event->seat->seat));
        Q_EMIT q->requestResize(seat, WTools::toQtEdge(event->edges), event->serial);
    });
    QObject::connect(handle(), &qw_xdg_toplevel::notify_request_maximize, q, [q, this] () {
        if ((*handle())->requested.maximized) {
            Q_EMIT q->requestMaximize();
        } else {
            Q_EMIT q->requestCancelMaximize();
        }
    });
    QObject::connect(handle(), &qw_xdg_toplevel::notify_request_minimize, q, [q, this] () {
        // Wayland clients can't request unset minimization on this surface
        if ((*handle())->requested.minimized) {
            Q_EMIT q->requestMinimize();
        }
    });
    QObject::connect(handle(), &qw_xdg_toplevel::notify_request_fullscreen, q, [q, this] () {
        if ((*handle())->requested.fullscreen) {
            Q_EMIT q->requestFullscreen();
        } else {
            Q_EMIT q->requestCancelFullscreen();
        }
    });
    QObject::connect(handle(), &qw_xdg_toplevel::notify_request_show_window_menu, q, [q] (wlr_xdg_toplevel_show_window_menu_event *event) {
        auto seat = WSeat::fromHandle(qw_seat::from(event->seat->seat));
        Q_EMIT q->requestShowWindowMenu(seat, QPoint(event->x, event->y), event->serial);
    });

    QObject::connect(handle(), &qw_xdg_toplevel::notify_set_parent, q, &WXdgToplevelSurface::parentXdgSurfaceChanged);
    QObject::connect(handle(), &qw_xdg_toplevel::notify_set_title, q, &WXdgToplevelSurface::titleChanged);
    QObject::connect(handle(), &qw_xdg_toplevel::notify_set_app_id, q, &WXdgToplevelSurface::appIdChanged);
}

WXdgToplevelSurface::WXdgToplevelSurface(qw_xdg_toplevel *handle, QObject *parent)
    : WXdgSurface(*new WXdgToplevelSurfacePrivate(this, handle), parent)
{
    d_func()->init();
}

WXdgToplevelSurface::~WXdgToplevelSurface()
{

}


bool WXdgToplevelSurface::hasCapability(Capability cap) const
{
    switch (cap) {
        using enum Capability;
    case Resize:
    case Focus:
    case Activate:
    case Maximized:
    case FullScreen:
        return true;
    default:
        break;
    }
    Q_UNREACHABLE();
}

WSurface *WXdgToplevelSurface::surface() const
{
    W_DC(WXdgToplevelSurface);
    return d->surface;
}

qw_xdg_toplevel *WXdgToplevelSurface::handle() const
{
    W_DC(WXdgToplevelSurface);
    return d->handle();
}

qw_surface *WXdgToplevelSurface::inputTargetAt(QPointF &localPos) const
{
    // find a wlr_suface object who can receive the events
    const QPointF pos = localPos;
    auto xdgSurface = qw_xdg_surface::from(handle()->handle()->base);
    auto sur = xdgSurface->surface_at(pos.x(), pos.y(), &localPos.rx(), &localPos.ry());
    return sur ? qw_surface::from(sur) : nullptr;
}

WXdgToplevelSurface *WXdgToplevelSurface::fromHandle(qw_xdg_toplevel *handle)
{
    return handle->get_data<WXdgToplevelSurface>();
}

WXdgToplevelSurface *WXdgToplevelSurface::fromSurface(WSurface *surface)
{
    return surface->getAttachedData<WXdgToplevelSurface>();
}

void WXdgToplevelSurface::resize(const QSize &size)
{
    handle()->set_size(size.width(), size.height());
}

void WXdgToplevelSurface::close()
{
    handle()->send_close();
}

bool WXdgToplevelSurface::isResizeing() const
{
    W_DC(WXdgToplevelSurface);
    return d->resizeing;
}

bool WXdgToplevelSurface::isActivated() const
{
    W_DC(WXdgToplevelSurface);
    return d->activated;
}

bool WXdgToplevelSurface::isMaximized() const
{
    W_DC(WXdgToplevelSurface);
    return d->maximized;
}

bool WXdgToplevelSurface::isMinimized() const
{
    W_DC(WXdgToplevelSurface);
    return d->minimized;
}

bool WXdgToplevelSurface::isFullScreen() const
{
    W_DC(WXdgToplevelSurface);
    return d->fullscreen;
}

QRect WXdgToplevelSurface::getContentGeometry() const
{
    auto xdgSurface = qw_xdg_surface::from(handle()->handle()->base);
    qw_box tmp = qw_box(xdgSurface->handle()->geometry);
    return tmp.toQRect();
}

QSize WXdgToplevelSurface::minSize() const
{
    W_DC(WXdgToplevelSurface);
    auto wtoplevel = d->nativeHandle();
    return QSize(wtoplevel->current.min_width,
                 wtoplevel->current.min_height);

}

QSize WXdgToplevelSurface::maxSize() const
{
    W_DC(WXdgToplevelSurface);
    auto wtoplevel = d->nativeHandle();
    return QSize(wtoplevel->current.max_width,
                 wtoplevel->current.max_height);

}

QString WXdgToplevelSurface::title() const
{
    W_DC(WXdgToplevelSurface);
    return QString::fromUtf8(d->nativeHandle()->title);
}

QString WXdgToplevelSurface::appId() const
{
    W_DC(WXdgToplevelSurface);
    return QString::fromLocal8Bit(d->nativeHandle()->app_id);
}

bool WXdgToplevelSurface::isInitialized() const
{
    W_DC(WXdgToplevelSurface);
    return d->nativeHandle()->base->initialized;
}

WXdgToplevelSurface *WXdgToplevelSurface::parentXdgSurface() const
{
    auto parent = handle()->handle()->parent;
    if (!parent)
        return nullptr;
    return fromHandle(qw_xdg_toplevel::from(parent));
}

WSurface *WXdgToplevelSurface::parentSurface() const
{
    auto parent = handle()->handle()->parent;
    if (!parent)
        return nullptr;
    return WSurface::fromHandle(parent->base->surface);
}

void WXdgToplevelSurface::setResizeing(bool resizeing)
{
    handle()->set_resizing(resizeing);
}

void WXdgToplevelSurface::setMaximize(bool on)
{
    handle()->set_maximized(on);
}

void WXdgToplevelSurface::setMinimize(bool on)
{
    W_D(WXdgToplevelSurface);

    if (d->minimized != on) {
        d->minimized = on;
        Q_EMIT minimizeChanged();
    }
}

void WXdgToplevelSurface::setActivate(bool on)
{
    handle()->set_activated(on);
}

void WXdgToplevelSurface::setFullScreen(bool on)
{
    handle()->set_fullscreen(on);
}

bool WXdgToplevelSurface::checkNewSize(const QSize &size, QSize *clipedSize)
{
    W_D(WXdgToplevelSurface);
    if (clipedSize)
        *clipedSize = size;

    bool ok = true;
    auto wtoplevel = d->nativeHandle();
    if (size.width() > wtoplevel->current.max_width
        && wtoplevel->current.max_width > 0) {
        if (clipedSize)
            clipedSize->setWidth(wtoplevel->current.max_width);
        ok = false;
    }
    if (size.height() > wtoplevel->current.max_height
        && wtoplevel->current.max_height > 0) {
        if (clipedSize)
            clipedSize->setHeight(wtoplevel->current.max_height);
        ok = false;
    }
    if (size.width() < wtoplevel->current.min_width) {
        if (clipedSize)
            clipedSize->setWidth(wtoplevel->current.min_width);
        ok = false;
    }
    if (size.height() < wtoplevel->current.min_height) {
        if (clipedSize)
            clipedSize->setHeight(wtoplevel->current.min_height);
        ok = false;
    }
    return ok;
}

WAYLIB_SERVER_END_NAMESPACE

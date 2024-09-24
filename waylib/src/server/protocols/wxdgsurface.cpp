// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgsurface.h"
#include "wseat.h"
#include "private/wtoplevelsurface_p.h"
#include <wtools.h>

#include <qwxdgshell.h>
#include <qwseat.h>
#include <qwcompositor.h>
#include <qwlayershellv1.h>
#include <qwbox.h>

#include <QDebug>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WXdgSurfacePrivate(WXdgSurface *qq, qw_xdg_surface *handle);
    ~WXdgSurfacePrivate();

    WWRAP_HANDLE_FUNCTIONS(qw_xdg_surface, wlr_xdg_surface)

    wl_client *waylandClient() const {
        return nativeHandle()->client->client;
    }

    bool isPopup() const {
        return nativeHandle()->role == WLR_XDG_SURFACE_ROLE_POPUP;
    }

    bool isToplevel() const {
        return nativeHandle()->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL;
    }

    // begin slot function
    void on_configure(wlr_xdg_surface_configure *event);
    void on_ack_configure(wlr_xdg_surface_configure *event);
    // end slot function

    void init();
    void connect();
    void updatePosition();

    void instantRelease();

    W_DECLARE_PUBLIC(WXdgSurface)

    WSurface *surface = nullptr;
    QPointF position;
    uint resizeing:1;
    uint activated:1;
    uint maximized:1;
    uint minimized:1;
    uint fullscreen:1;
};

WXdgSurfacePrivate::WXdgSurfacePrivate(WXdgSurface *qq, qw_xdg_surface *hh)
    : WToplevelSurfacePrivate(qq)
    , resizeing(false)
    , activated(false)
    , maximized(false)
    , minimized(false)
    , fullscreen(false)
{
    initHandle(hh);
}

WXdgSurfacePrivate::~WXdgSurfacePrivate()
{

}

void WXdgSurfacePrivate::instantRelease()
{
    if (!surface)
        return;
    W_Q(WXdgSurface);
    handle()->set_data(nullptr, nullptr);
    handle()->disconnect(q);
    if (isToplevel()) {
        auto toplevel = qw_xdg_toplevel::from(nativeHandle()->toplevel);
        toplevel->disconnect(q);
    }
    surface->safeDeleteLater();
    surface = nullptr;
}

void WXdgSurfacePrivate::on_configure(wlr_xdg_surface_configure *event)
{
    if (isToplevel()) {
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

        if (event->toplevel_configure->fullscreen != fullscreen) {
            fullscreen = event->toplevel_configure->fullscreen;
            Q_EMIT q->fullscreenChanged();
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
    handle()->set_data(this, q);

    Q_ASSERT(!q->surface());
    surface = new WSurface(qw_surface::from(nativeHandle()->surface), q);
    surface->setAttachedData<WXdgSurface>(q);

    connect();
}

void WXdgSurfacePrivate::connect()
{
    W_Q(WXdgSurface);

    QObject::connect(handle(), &qw_xdg_surface::notify_configure, q, [this] (wlr_xdg_surface_configure *event) {
        on_configure(event);
    });
    QObject::connect(handle(), &qw_xdg_surface::notify_ack_configure, q, [this] (wlr_xdg_surface_configure *event) {
        on_ack_configure(event);
    });

    // TODO: use safeConnect for toplevel
    if (isToplevel()) {
        auto toplevel = qw_xdg_toplevel::from(nativeHandle()->toplevel);
        QObject::connect(toplevel, &qw_xdg_toplevel::notify_request_move, q, [q] (wlr_xdg_toplevel_move_event *event) {
            auto seat = WSeat::fromHandle(qw_seat::from(event->seat->seat));
            Q_EMIT q->requestMove(seat, event->serial);
        });
        QObject::connect(toplevel, &qw_xdg_toplevel::notify_request_resize, q, [q] (wlr_xdg_toplevel_resize_event *event) {
            auto seat = WSeat::fromHandle(qw_seat::from(event->seat->seat));
            Q_EMIT q->requestResize(seat, WTools::toQtEdge(event->edges), event->serial);
        });
        QObject::connect(toplevel, &qw_xdg_toplevel::notify_request_maximize, q, [q, toplevel] () {
            if ((*toplevel)->requested.maximized) {
                Q_EMIT q->requestMaximize();
            } else {
                Q_EMIT q->requestCancelMaximize();
            }
        });
        QObject::connect(toplevel, &qw_xdg_toplevel::notify_request_minimize, q, [q, toplevel] () {
            if ((*toplevel)->requested.minimized) {
                Q_EMIT q->requestMinimize();
            } else {
                Q_EMIT q->requestCancelMinimize();
            }
        });
        QObject::connect(toplevel, &qw_xdg_toplevel::notify_request_fullscreen, q, [q, toplevel] () {
            if ((*toplevel)->requested.fullscreen) {
                Q_EMIT q->requestFullscreen();
            } else {
                Q_EMIT q->requestCancelFullscreen();
            }
        });
        QObject::connect(toplevel, &qw_xdg_toplevel::notify_request_show_window_menu, q, [q] (wlr_xdg_toplevel_show_window_menu_event *event) {
            auto seat = WSeat::fromHandle(qw_seat::from(event->seat->seat));
            Q_EMIT q->requestShowWindowMenu(seat, QPoint(event->x, event->y), event->serial);
        });

        QObject::connect(toplevel, &qw_xdg_toplevel::notify_set_parent, q, &WXdgSurface::parentXdgSurfaceChanged);

        QObject::connect(toplevel, &qw_xdg_toplevel::notify_set_title, q, &WXdgSurface::titleChanged);
        QObject::connect(toplevel, &qw_xdg_toplevel::notify_set_app_id, q, &WXdgSurface::appIdChanged);
    }
}

WXdgSurface::WXdgSurface(qw_xdg_surface *handle, QObject *parent)
    : WToplevelSurface(*new WXdgSurfacePrivate(this, handle), parent)
{
    d_func()->init();
}

WXdgSurface::~WXdgSurface()
{

}

bool WXdgSurface::isPopup() const
{
    W_DC(WXdgSurface);
    return d->isPopup();
}

bool WXdgSurface::isToplevel() const
{
    W_DC(WXdgSurface);
    return d->isToplevel();
}

bool WXdgSurface::doesNotAcceptFocus() const
{
    W_DC(WXdgSurface);
    return d->nativeHandle()->role == WLR_XDG_SURFACE_ROLE_NONE;
}

WSurface *WXdgSurface::surface() const
{
    W_DC(WXdgSurface);
    return d->surface;
}

qw_xdg_surface *WXdgSurface::handle() const
{
    W_DC(WXdgSurface);
    return d->handle();
}

qw_surface *WXdgSurface::inputTargetAt(QPointF &localPos) const
{
    W_DC(WXdgSurface);
    // find a wlr_suface object who can receive the events
    const QPointF pos = localPos;
    auto sur = d->handle()->surface_at(pos.x(), pos.y(), &localPos.rx(), &localPos.ry());
    return sur ? qw_surface::from(sur) : nullptr;
}

WXdgSurface *WXdgSurface::fromHandle(qw_xdg_surface *handle)
{
    return handle->get_data<WXdgSurface>();
}

WXdgSurface *WXdgSurface::fromSurface(WSurface *surface)
{
    return surface->getAttachedData<WXdgSurface>();
}

void WXdgSurface::resize(const QSize &size)
{
    W_D(WXdgSurface);

    if (isToplevel()) {
        auto toplevel = qw_xdg_toplevel::from(d->nativeHandle()->toplevel);
        toplevel->set_size(size.width(), size.height());
    }
}

void WXdgSurface::close()
{
    W_D(WXdgSurface);

    if (Q_LIKELY(isToplevel())) {
        auto toplevel = qw_xdg_toplevel::from(d->nativeHandle()->toplevel);
        toplevel->send_close();
    } else if (Q_LIKELY(isPopup())) {
        // wlr_xdg_popup_destroy will send popup_done to the client
        // can't use delete qw_xdg_popup, which is not owner of wlr_xdg_popup
        wlr_xdg_popup_destroy(d->nativeHandle()->popup);
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

bool WXdgSurface::isFullScreen() const
{
    W_DC(WXdgSurface);
    return d->fullscreen;
}

QRect WXdgSurface::getContentGeometry() const
{
    W_DC(WXdgSurface);
    qw_box tmp;
    d->handle()->get_geometry(tmp);
    return tmp.toQRect();
}

QSize WXdgSurface::minSize() const
{
    W_DC(WXdgSurface);
    if (isToplevel()) {
        auto wtoplevel = d->nativeHandle()->toplevel;
        return QSize(wtoplevel->current.min_width,
                     wtoplevel->current.min_height);
    }
    return QSize();
}

QSize WXdgSurface::maxSize() const
{
    W_DC(WXdgSurface);
    if (isToplevel()) {
        auto wtoplevel = d->nativeHandle()->toplevel;
        return QSize(wtoplevel->current.max_width,
                     wtoplevel->current.max_height);
    }

    return QSize();
}

QString WXdgSurface::title() const
{
    W_DC(WXdgSurface);
    return QString::fromUtf8(d->nativeHandle()->toplevel->title);
}

QString WXdgSurface::appId() const
{
    W_DC(WXdgSurface);
    return QString::fromLocal8Bit(d->nativeHandle()->toplevel->app_id);
}

WXdgSurface *WXdgSurface::parentXdgSurface() const
{
    W_DC(WXdgSurface);

    if (isToplevel()) {
        auto parent = d->nativeHandle()->toplevel->parent;
        if (!parent)
            return nullptr;
        return fromHandle(qw_xdg_surface::from(parent->base));
    }

    return nullptr;
}

WSurface *WXdgSurface::parentSurface() const
{
    W_DC(WXdgSurface);
    if (isToplevel()) {
        auto parent = d->nativeHandle()->toplevel->parent;
        if (!parent)
            return nullptr;
        return WSurface::fromHandle(parent->base->surface);
    } else if (isPopup()) {
        auto parent = d->nativeHandle()->popup->parent;
        if (!parent)
            return nullptr;
        return WSurface::fromHandle(parent);
    }
    return nullptr;
}

QPointF WXdgSurface::getPopupPosition() const
{
    auto wpopup = handle()->handle()->popup;
    Q_ASSERT(wpopup);
    if (wpopup->parent && qw_xdg_surface::try_from_wlr_surface(wpopup->parent)) {
        auto popup = qw_xdg_popup::from(wpopup);
        double popup_sx, popup_sy;
        popup->get_position(&popup_sx, &popup_sy);
        return QPointF(popup_sx, popup_sy);
    }
    return {static_cast<qreal>(wpopup->current.geometry.x),
            static_cast<qreal>(wpopup->current.geometry.y)};
}

void WXdgSurface::setResizeing(bool resizeing)
{
    W_D(WXdgSurface);
    if (isToplevel()) {
        auto toplevel = qw_xdg_toplevel::from(d->nativeHandle()->toplevel);
        toplevel->set_resizing(resizeing);
    }
}

void WXdgSurface::setMaximize(bool on)
{
    W_D(WXdgSurface);
    if (isToplevel()) {
        auto toplevel = qw_xdg_toplevel::from(d->nativeHandle()->toplevel);
        toplevel->set_maximized(on);
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
    if (isToplevel()) {
        auto toplevel = qw_xdg_toplevel::from(d->nativeHandle()->toplevel);
        toplevel->set_activated(on);
    }
}

void WXdgSurface::setFullScreen(bool on)
{
    W_D(WXdgSurface);

    if (isToplevel()) {
        auto toplevel = qw_xdg_toplevel::from(d->nativeHandle()->toplevel);
        toplevel->set_fullscreen(on);
    }
}

bool WXdgSurface::checkNewSize(const QSize &size)
{
    W_D(WXdgSurface);
    if (isToplevel()) {
        auto wtoplevel = d->nativeHandle()->toplevel;
        if (size.width() > wtoplevel->current.max_width
            && wtoplevel->current.max_width > 0)
            return false;
        if (size.height() > wtoplevel->current.max_height
            && wtoplevel->current.max_height > 0)
            return false;
        if (size.width() < wtoplevel->current.min_width
            && wtoplevel->current.min_width> 0)
            return false;
        if (size.height() < wtoplevel->current.min_height
            && wtoplevel->current.min_height > 0)
            return false;
        return true;
    }

    return false;
}

WAYLIB_SERVER_END_NAMESPACE

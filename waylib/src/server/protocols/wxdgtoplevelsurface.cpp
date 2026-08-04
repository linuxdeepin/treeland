// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgtoplevelsurface.h"

#include "private/wtoplevelsurface_p.h"
#include "wseat.h"
#include "wtools.h"

#include <climits>

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using ToplevelRegistry = QHash<const wlr_xdg_toplevel *, WXdgToplevelSurface *>;
Q_GLOBAL_STATIC(ToplevelRegistry, s_toplevels)

class Q_DECL_HIDDEN WXdgToplevelSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WXdgToplevelSurfacePrivate(WXdgToplevelSurface *qq, wlr_xdg_toplevel *handle);
    ~WXdgToplevelSurfacePrivate();

    inline wlr_xdg_toplevel *handle() const { return toplevelHandle; }

    wl_client *waylandClient() const override {
        return toplevelHandle ? toplevelHandle->base->client->client : nullptr;
    }

    // begin slot function
    void on_configure(wlr_xdg_surface_configure *event);
    void on_ack_configure(wlr_xdg_surface_configure *event);
    // end slot function

    void init();
    void connectNativeEvents();
    void disconnectNativeEvents();

    void instantRelease() override;
    void updateSizeFromCommit();

    W_DECLARE_PUBLIC(WXdgToplevelSurface)

    wlr_xdg_toplevel *toplevelHandle = nullptr;
    WNativeListener destroyListener;
    WNativeListener configureListener;
    WNativeListener requestMoveListener;
    WNativeListener requestResizeListener;
    WNativeListener requestMaximizeListener;
    WNativeListener requestMinimizeListener;
    WNativeListener requestFullscreenListener;
    WNativeListener requestShowWindowMenuListener;
    WNativeListener setParentListener;
    WNativeListener setTitleListener;
    WNativeListener setAppIdListener;
    WSurface *surface = nullptr;
    QPointF position;
    uint resizeing:1;
    uint activated:1;
    uint maximized:1;
    uint minimized:1;
    uint fullscreen:1;
    QSize minimumSize;
    QSize maximumSize = QSize(INT_MAX, INT_MAX);

    QString tag;
    QString description;
};

WXdgToplevelSurfacePrivate::WXdgToplevelSurfacePrivate(WXdgToplevelSurface *qq, wlr_xdg_toplevel *handle)
    : WToplevelSurfacePrivate(qq)
    , toplevelHandle(handle)
    , resizeing(false)
    , activated(false)
    , maximized(false)
    , minimized(false)
    , fullscreen(false)
{
    Q_ASSERT(toplevelHandle);
    Q_ASSERT(!s_toplevels->contains(toplevelHandle));
    s_toplevels->insert(toplevelHandle, qq);
}

WXdgToplevelSurfacePrivate::~WXdgToplevelSurfacePrivate()
{

}

void WXdgToplevelSurfacePrivate::instantRelease()
{
    W_Q(WXdgToplevelSurface);
    if (toplevelHandle) {
        disconnectNativeEvents();
        s_toplevels->remove(toplevelHandle);
        toplevelHandle = nullptr;
    }
    if (surface) {
        surface->safeDeleteLater();
        surface = nullptr;
    }
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

void WXdgToplevelSurfacePrivate::updateSizeFromCommit()
{
    W_Q(WXdgToplevelSurface);

    const QSize minimumSize(qMax(0, handle()->current.min_width),
                            qMax(0, handle()->current.min_height));
    const QSize maximumSize(
        handle()->current.max_width > 0 ? handle()->current.max_width : INT_MAX,
        handle()->current.max_height > 0 ? handle()->current.max_height : INT_MAX);

    if (this->minimumSize != minimumSize) {
        this->minimumSize = minimumSize;
        Q_EMIT q->minimumSizeChanged(this->minimumSize);
    }

    if (this->maximumSize != maximumSize) {
        this->maximumSize = maximumSize;
        Q_EMIT q->maximumSizeChanged(this->maximumSize);
    }
}

void WXdgToplevelSurfacePrivate::init()
{
    W_Q(WXdgToplevelSurface);
    Q_ASSERT(!q->surface());
    surface = new WSurface(handle()->base->surface, q);
    surface->setAttachedData<WXdgToplevelSurface>(q);

    connectNativeEvents();
}

void WXdgToplevelSurfacePrivate::connectNativeEvents()
{
    W_Q(WXdgToplevelSurface);

    q->surface()->safeConnect(&WSurface::commit, q, [this] {
        updateSizeFromCommit();
    });
    destroyListener.connect(&handle()->events.destroy, [q](void *) {
        q->safeDeleteLater();
    });
    configureListener.connect(&handle()->base->events.configure, [this](void *data) {
        on_configure(static_cast<wlr_xdg_surface_configure *>(data));
    });
    requestMoveListener.connect(&handle()->events.request_move, [q](void *data) {
        auto *event = static_cast<wlr_xdg_toplevel_move_event *>(data);
        auto seat = WSeat::fromHandle(event->seat->seat);
        Q_EMIT q->requestMove(seat, event->serial);
    });
    requestResizeListener.connect(&handle()->events.request_resize, [q](void *data) {
        auto *event = static_cast<wlr_xdg_toplevel_resize_event *>(data);
        auto seat = WSeat::fromHandle(event->seat->seat);
        Q_EMIT q->requestResize(seat, WTools::toQtEdge(event->edges), event->serial);
    });
    requestMaximizeListener.connect(&handle()->events.request_maximize, [q, this](void *) {
        if (handle()->requested.maximized) {
            Q_EMIT q->requestMaximize();
        } else {
            Q_EMIT q->requestCancelMaximize();
        }
    });
    requestMinimizeListener.connect(&handle()->events.request_minimize, [q, this](void *) {
        // Wayland clients can't request unset minimization on this surface
        if (handle()->requested.minimized) {
            Q_EMIT q->requestMinimize();
        }
    });
    requestFullscreenListener.connect(&handle()->events.request_fullscreen, [q, this](void *) {
        if (handle()->requested.fullscreen) {
            Q_EMIT q->requestFullscreen();
        } else {
            Q_EMIT q->requestCancelFullscreen();
        }
    });
    requestShowWindowMenuListener.connect(&handle()->events.request_show_window_menu, [q](void *data) {
        auto *event = static_cast<wlr_xdg_toplevel_show_window_menu_event *>(data);
        auto seat = WSeat::fromHandle(event->seat->seat);
        Q_EMIT q->requestShowWindowMenu(seat, QPoint(event->x, event->y), event->serial);
    });
    setParentListener.connect(&handle()->events.set_parent, [q](void *) {
        Q_EMIT q->parentXdgSurfaceChanged();
    });
    setTitleListener.connect(&handle()->events.set_title, [q](void *) {
        Q_EMIT q->titleChanged();
    });
    setAppIdListener.connect(&handle()->events.set_app_id, [q](void *) {
        Q_EMIT q->appIdChanged();
    });
}

void WXdgToplevelSurfacePrivate::disconnectNativeEvents()
{
    destroyListener.disconnect();
    configureListener.disconnect();
    requestMoveListener.disconnect();
    requestResizeListener.disconnect();
    requestMaximizeListener.disconnect();
    requestMinimizeListener.disconnect();
    requestFullscreenListener.disconnect();
    requestShowWindowMenuListener.disconnect();
    setParentListener.disconnect();
    setTitleListener.disconnect();
    setAppIdListener.disconnect();
}

WXdgToplevelSurface::WXdgToplevelSurface(wlr_xdg_toplevel *handle, QObject *parent)
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
    case Resize: {
        const QSize min = minSize();
        const QSize max = maxSize();
        return min.width() < max.width() || min.height() < max.height();
    }
    case Maximized: {
        const QSize min = minSize();
        const QSize max = maxSize();
        return min.width() < max.width() && min.height() < max.height();
    }
    case Focus:
    case Activate:
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

wlr_xdg_toplevel *WXdgToplevelSurface::handle() const
{
    W_DC(WXdgToplevelSurface);
    return d->handle();
}

wlr_surface *WXdgToplevelSurface::inputTargetAt(QPointF &localPos) const
{
    // find a wlr_suface object who can receive the events
    const QPointF pos = localPos;
    return wlr_xdg_surface_surface_at(handle()->base, pos.x(), pos.y(),
                                      &localPos.rx(), &localPos.ry());
}

WXdgToplevelSurface *WXdgToplevelSurface::fromHandle(wlr_xdg_toplevel *handle)
{
    return s_toplevels->value(handle);
}

WXdgToplevelSurface *WXdgToplevelSurface::fromSurface(WSurface *surface)
{
    return surface->getAttachedData<WXdgToplevelSurface>();
}

void WXdgToplevelSurface::resize(const QSize &size)
{
    wlr_xdg_toplevel_set_size(handle(), size.width(), size.height());
}

void WXdgToplevelSurface::close()
{
    wlr_xdg_toplevel_send_close(handle());
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
    const auto &geometry = handle()->base->geometry;
    return QRect(geometry.x, geometry.y, geometry.width, geometry.height);
}

QSize WXdgToplevelSurface::minSize() const
{
    W_DC(WXdgToplevelSurface);
    return d->minimumSize;
}

QSize WXdgToplevelSurface::maxSize() const
{
    W_DC(WXdgToplevelSurface);
    return d->maximumSize;
}

QString WXdgToplevelSurface::title() const
{
    W_DC(WXdgToplevelSurface);
    return QString::fromUtf8(d->handle()->title);
}

QString WXdgToplevelSurface::appId() const
{
    W_DC(WXdgToplevelSurface);
    return QString::fromLocal8Bit(d->handle()->app_id);
}

QString WXdgToplevelSurface::tag() const
{
    W_DC(WXdgToplevelSurface);
    return d->tag;
}

QString WXdgToplevelSurface::description() const
{
    W_DC(WXdgToplevelSurface);
    return d->description;
}

bool WXdgToplevelSurface::isInitialized() const
{
    W_DC(WXdgToplevelSurface);
    return d->handle()->base->initialized;
}

WXdgToplevelSurface *WXdgToplevelSurface::parentXdgSurface() const
{
    auto parent = handle()->parent;
    if (!parent)
        return nullptr;
    return fromHandle(parent);
}

WSurface *WXdgToplevelSurface::parentSurface() const
{
    auto parent = handle()->parent;
    if (!parent)
        return nullptr;
    return WSurface::fromHandle(parent->base->surface);
}

void WXdgToplevelSurface::setTag(const QString &tag)
{
    W_D(WXdgToplevelSurface);
    if (d->tag == tag)
        return;
    d->tag = tag;
    Q_EMIT tagChanged();
}

void WXdgToplevelSurface::setDescription(const QString &description)
{
    W_D(WXdgToplevelSurface);
    if (d->description == description)
        return;
    d->description = description;
    Q_EMIT descriptionChanged();
}

void WXdgToplevelSurface::setResizeing(bool resizeing)
{
    wlr_xdg_toplevel_set_resizing(handle(), resizeing);
}

void WXdgToplevelSurface::setMaximize(bool on)
{
    wlr_xdg_toplevel_set_maximized(handle(), on);
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
    wlr_xdg_toplevel_set_activated(handle(), on);
}

void WXdgToplevelSurface::setFullScreen(bool on)
{
    wlr_xdg_toplevel_set_fullscreen(handle(), on);
}

bool WXdgToplevelSurface::checkNewSize(const QSize &size, QSize *clipedSize)
{
    W_D(WXdgToplevelSurface);
    if (clipedSize)
        *clipedSize = size;

    bool ok = true;
    auto wtoplevel = d->handle();
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

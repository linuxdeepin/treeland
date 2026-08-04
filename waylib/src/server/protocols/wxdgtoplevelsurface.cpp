// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgtoplevelsurface.h"

#include "private/wtoplevelsurface_p.h"
#include "wseat.h"
#include "wtools.h"

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>

#include <climits>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgToplevelSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WXdgToplevelSurfacePrivate(WXdgToplevelSurface *qq, wlr_xdg_toplevel *handle);
    ~WXdgToplevelSurfacePrivate();

    WWRAP_NATIVE_HANDLE_FUNCTIONS(wlr_xdg_toplevel)

    wl_client *waylandClient() const override {
        return handle()->base->client->client;
    }

    // begin slot function
    void on_configure(wlr_xdg_surface_configure *event);
    // end slot function

    void init();
    void connect();
    void removeAllListeners();

    void instantRelease() override;
    void updateSizeFromCommit();

    W_DECLARE_PUBLIC(WXdgToplevelSurface)

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

    WScopedListener m_configureListener;
    WScopedListener m_requestMoveListener;
    WScopedListener m_requestResizeListener;
    WScopedListener m_requestMaximizeListener;
    WScopedListener m_requestMinimizeListener;
    WScopedListener m_requestFullscreenListener;
    WScopedListener m_requestShowWindowMenuListener;
    WScopedListener m_setParentListener;
    WScopedListener m_setTitleListener;
    WScopedListener m_setAppIdListener;
};

WXdgToplevelSurfacePrivate::WXdgToplevelSurfacePrivate(WXdgToplevelSurface *qq, wlr_xdg_toplevel *hh)
    : WToplevelSurfacePrivate(qq)
    , resizeing(false)
    , activated(false)
    , maximized(false)
    , minimized(false)
    , fullscreen(false)
{
    initNativeHandle(hh, &hh->events.destroy);
}

WXdgToplevelSurfacePrivate::~WXdgToplevelSurfacePrivate()
{

}

void WXdgToplevelSurfacePrivate::removeAllListeners()
{
    m_configureListener.remove();
    m_requestMoveListener.remove();
    m_requestResizeListener.remove();
    m_requestMaximizeListener.remove();
    m_requestMinimizeListener.remove();
    m_requestFullscreenListener.remove();
    m_requestShowWindowMenuListener.remove();
    m_setParentListener.remove();
    m_setTitleListener.remove();
    m_setAppIdListener.remove();
}

void WXdgToplevelSurfacePrivate::instantRelease()
{
    if (!surface)
        return;

    removeAllListeners();
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

    connect();
}

void WXdgToplevelSurfacePrivate::connect()
{
    W_Q(WXdgToplevelSurface);
    wlr_xdg_toplevel *toplevel = handle();
    wlr_xdg_surface *xdgSurface = toplevel->base;

    q->surface()->safeConnect(&WSurface::commit, q, [this] {
        updateSizeFromCommit();
    });

    m_configureListener.connect(&xdgSurface->events.configure, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WXdgToplevelSurfacePrivate, &WXdgToplevelSurfacePrivate::m_configureListener>(listener);
        self->on_configure(static_cast<wlr_xdg_surface_configure*>(data));
    });

    m_requestMoveListener.connect(&toplevel->events.request_move, [q](wl_listener *listener, void *data) {
        (void)listener;
        auto *event = static_cast<wlr_xdg_toplevel_move_event*>(data);
        auto seat = WSeat::fromHandle(event->seat->seat);
        Q_EMIT q->requestMove(seat, event->serial);
    });

    m_requestResizeListener.connect(&toplevel->events.request_resize, [q](wl_listener *listener, void *data) {
        (void)listener;
        auto *event = static_cast<wlr_xdg_toplevel_resize_event*>(data);
        auto seat = WSeat::fromHandle(event->seat->seat);
        Q_EMIT q->requestResize(seat, WTools::toQtEdge(event->edges), event->serial);
    });

    m_requestMaximizeListener.connect(&toplevel->events.request_maximize, [q, this](wl_listener *listener, void *) {
        (void)listener;
        if (handle()->requested.maximized) {
            Q_EMIT q->requestMaximize();
        } else {
            Q_EMIT q->requestCancelMaximize();
        }
    });

    m_requestMinimizeListener.connect(&toplevel->events.request_minimize, [q, this](wl_listener *listener, void *) {
        (void)listener;
        if (handle()->requested.minimized) {
            Q_EMIT q->requestMinimize();
        }
    });

    m_requestFullscreenListener.connect(&toplevel->events.request_fullscreen, [q, this](wl_listener *listener, void *) {
        (void)listener;
        if (handle()->requested.fullscreen) {
            Q_EMIT q->requestFullscreen();
        } else {
            Q_EMIT q->requestCancelFullscreen();
        }
    });

    m_requestShowWindowMenuListener.connect(&toplevel->events.request_show_window_menu, [q](wl_listener *listener, void *data) {
        (void)listener;
        auto *event = static_cast<wlr_xdg_toplevel_show_window_menu_event*>(data);
        auto seat = WSeat::fromHandle(event->seat->seat);
        Q_EMIT q->requestShowWindowMenu(seat, QPoint(event->x, event->y), event->serial);
    });

    m_setParentListener.connect(&toplevel->events.set_parent, [q](wl_listener *listener, void *) {
        (void)listener;
        Q_EMIT q->parentXdgSurfaceChanged();
    });

    m_setTitleListener.connect(&toplevel->events.set_title, [q](wl_listener *listener, void *) {
        (void)listener;
        Q_EMIT q->titleChanged();
    });

    m_setAppIdListener.connect(&toplevel->events.set_app_id, [q](wl_listener *listener, void *) {
        (void)listener;
        Q_EMIT q->appIdChanged();
    });
}

WXdgToplevelSurface::WXdgToplevelSurface(wlr_xdg_toplevel *handle, QObject *parent)
    : WXdgSurface(*new WXdgToplevelSurfacePrivate(this, handle), parent)
{
    W_D(WXdgToplevelSurface);
    d->init();
}

WXdgToplevelSurface::~WXdgToplevelSurface()
{
}

bool WXdgToplevelSurface::hasCapability(Capability cap) const
{
    return cap == Capability::Keyboard || cap == Capability::Pointer
        || cap == Capability::Resize || cap == Capability::Focus
        || cap == Capability::Activate || cap == Capability::Maximized
        || cap == Capability::FullScreen;
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
    // find a wlr_surface object who can receive the events
    const QPointF pos = localPos;
    auto sur = wlr_xdg_surface_surface_at(d_func()->handle()->base, pos.x(), pos.y(), &localPos.rx(), &localPos.ry());
    return sur;
}

WXdgToplevelSurface *WXdgToplevelSurface::fromHandle(wlr_xdg_toplevel *handle)
{
    return static_cast<WXdgToplevelSurface*>(WWrapObjectPrivate::fromNativeHandle(handle));
}

WXdgToplevelSurface *WXdgToplevelSurface::fromSurface(WSurface *surface)
{
    return surface->getAttachedData<WXdgToplevelSurface>();
}

void WXdgToplevelSurface::resize(const QSize &size)
{
    wlr_xdg_toplevel_set_size(d_func()->handle(), size.width(), size.height());
}

void WXdgToplevelSurface::close()
{
    wlr_xdg_toplevel_send_close(d_func()->handle());
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
    auto xdgSurface = d_func()->handle()->base;
    return QRect(xdgSurface->geometry.x, xdgSurface->geometry.y,
                 xdgSurface->geometry.width, xdgSurface->geometry.height);
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
    auto parent = d_func()->handle()->parent;
    if (!parent)
        return nullptr;
    return fromHandle(parent);
}

WSurface *WXdgToplevelSurface::parentSurface() const
{
    auto parent = d_func()->handle()->parent;
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
    wlr_xdg_toplevel_set_resizing(d_func()->handle(), resizeing);
}

void WXdgToplevelSurface::setMaximize(bool on)
{
    wlr_xdg_toplevel_set_maximized(d_func()->handle(), on);
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
    wlr_xdg_toplevel_set_activated(d_func()->handle(), on);
}

void WXdgToplevelSurface::setFullScreen(bool on)
{
    wlr_xdg_toplevel_set_fullscreen(d_func()->handle(), on);
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

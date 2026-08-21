// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgtoplevelsurface.h"

#include "private/wtoplevelsurface_p.h"
#include "woutput.h"
#include "wseat.h"
#include "wscoplistener.h"
#include "wtools.h"

#include <wlr_all.h>

#include <climits>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgToplevelSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WXdgToplevelSurfacePrivate(WXdgToplevelSurface *qq, wlr_xdg_toplevel *handle);
    ~WXdgToplevelSurfacePrivate();

    inline wlr_xdg_toplevel *handle() const {
        return m_handle;
    }

    wl_client *waylandClient() const override {
        return handle()->base->client->client;
    }

    // begin slot function
    void on_configure(wlr_xdg_surface_configure *event);
    void on_ack_configure(wlr_xdg_surface_configure *event);
    // end slot function

    void init();
    void connect();

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

private:
    // The xdg owner destroys this handle after notifying the wrapper.
    // Keep the address stable through that callback.
    wlr_xdg_toplevel *m_handle = nullptr;
};

WXdgToplevelSurfacePrivate::WXdgToplevelSurfacePrivate(WXdgToplevelSurface *qq, wlr_xdg_toplevel *hh)
    : WToplevelSurfacePrivate(qq)
    , resizeing(false)
    , activated(false)
    , maximized(false)
    , minimized(false)
    , fullscreen(false)
{
    Q_ASSERT(hh);
    // wlr_xdg_toplevel has no data field; store on the xdg_surface instead
    // (a surface has exactly one role, so no clash with popups).
    hh->base->data = qq;
    m_handle = hh;
}

WXdgToplevelSurfacePrivate::~WXdgToplevelSurfacePrivate()
{

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
    surface = new WSurface(m_handle->base->surface);
    surface->setAttachedData<WXdgToplevelSurface>(q);

    connect();
}

void WXdgToplevelSurfacePrivate::connect()
{
    W_Q(WXdgToplevelSurface);

    QObject::connect(q->surface(), &WSurface::commit, q, [this] {
        updateSizeFromCommit();
    });

    auto *xdgSurface = m_handle->base;
    q->listeners()->add(&xdgSurface->events.configure, this,
        &WXdgToplevelSurfacePrivate::on_configure);

    // TODO: use safeConnect for toplevel
    q->listeners()->add(&m_handle->events.request_move, q,
        [q] (wlr_xdg_toplevel_move_event *event) {
        auto seat = WSeat::fromHandle(event->seat->seat);
        Q_EMIT q->requestMove(seat, event->serial);
    });
    q->listeners()->add(&m_handle->events.request_resize, q,
        [q] (wlr_xdg_toplevel_resize_event *event) {
        auto seat = WSeat::fromHandle(event->seat->seat);
        Q_EMIT q->requestResize(seat, WTools::toQtEdge(event->edges), event->serial);
    });
    q->listeners()->add(&m_handle->events.request_maximize, q,
        [q, this] (void *) {
        if (m_handle->requested.maximized) {
            Q_EMIT q->requestMaximize();
        } else {
            Q_EMIT q->requestCancelMaximize();
        }
    });
    q->listeners()->add(&m_handle->events.request_minimize, q,
        [q, this] (void *) {
        // Wayland clients can't request unset minimization on this surface
        if (m_handle->requested.minimized) {
            Q_EMIT q->requestMinimize();
        }
    });
    q->listeners()->add(&m_handle->events.request_fullscreen, q,
        [q, this] (void *) {
        if (m_handle->requested.fullscreen) {
            WOutput *output = m_handle->requested.fullscreen_output
                ? WOutput::fromHandle(m_handle->requested.fullscreen_output)
                : nullptr;
            Q_EMIT q->requestFullscreen(output);
        } else {
            Q_EMIT q->requestCancelFullscreen();
        }
    });
    q->listeners()->add(&m_handle->events.request_show_window_menu, q,
        [q] (wlr_xdg_toplevel_show_window_menu_event *event) {
        auto seat = WSeat::fromHandle(event->seat->seat);
        Q_EMIT q->requestShowWindowMenu(seat, QPoint(event->x, event->y), event->serial);
    });

    q->listeners()->add(&m_handle->events.set_parent, q, &WXdgToplevelSurface::parentXdgSurfaceChanged);
    q->listeners()->add(&m_handle->events.set_title, q, &WXdgToplevelSurface::titleChanged);
    q->listeners()->add(&m_handle->events.set_app_id, q, &WXdgToplevelSurface::appIdChanged);
}

WXdgToplevelSurface::WXdgToplevelSurface(wlr_xdg_toplevel *handle)
    : WXdgSurface(*new WXdgToplevelSurfacePrivate(this, handle))
{
    d_func()->init();
}

WXdgToplevelSurface::~WXdgToplevelSurface()
{
    teardown();
    // Notify listeners while the object is still usable (its members are
    // alive during the destructor body).
    Q_EMIT beforeDestroy();
    // Owner rule: this object created the WSurface wrapper, release it.
    W_D(WXdgToplevelSurface);
    // Clear the reverse fromHandle() mapping. The shell destroys this
    // wrapper from the toplevel's destroy callback (or while the native
    // handle is still alive), so the handle is valid here.
    if (d->m_handle && d->m_handle->base->data == this)
        d->m_handle->base->data = nullptr;
    delete d->surface;
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
    auto *xdgSurface = handle()->base;
    auto sur = wlr_xdg_surface_surface_at(xdgSurface, pos.x(), pos.y(), &localPos.rx(), &localPos.ry());
    return sur;
}

WXdgToplevelSurface *WXdgToplevelSurface::fromHandle(wlr_xdg_toplevel *handle)
{
    if (!handle)
        return nullptr;
    return static_cast<WXdgToplevelSurface*>(handle->base->data);
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
    auto *xdgSurface = handle()->base;
    const wlr_box &tmp = xdgSurface->geometry;
    return QRect(tmp.x, tmp.y, tmp.width, tmp.height);
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

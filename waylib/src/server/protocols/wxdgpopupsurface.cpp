// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgpopupsurface.h"
#include "private/wtoplevelsurface_p.h"
#include "wscoplistener.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgPopupSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WXdgPopupSurfacePrivate(WXdgPopupSurface *qq, wlr_xdg_popup *handle);
    ~WXdgPopupSurfacePrivate();

    inline wlr_xdg_popup *handle() const {
        return m_handle;
    }

    wl_client *waylandClient() const override {
        return handle()->base->client->client;
    }

    void init();
    void connect();

    W_DECLARE_PUBLIC(WXdgPopupSurface)

    WSurface *surface = nullptr;
    QPointF position;

private:
    // The xdg owner destroys this handle after notifying the wrapper.
    // Keep the address stable through that callback.
    wlr_xdg_popup *m_handle = nullptr;
};

WXdgPopupSurfacePrivate::WXdgPopupSurfacePrivate(WXdgPopupSurface *qq, wlr_xdg_popup *hh)
    : WToplevelSurfacePrivate(qq)
{
    Q_ASSERT(hh);
    // wlr_xdg_popup has no data field; store on the xdg_surface instead.
    hh->base->data = qq;
    m_handle = hh;
}

WXdgPopupSurfacePrivate::~WXdgPopupSurfacePrivate()
{

}

void WXdgPopupSurfacePrivate::init()
{
    W_Q(WXdgPopupSurface);
    Q_ASSERT(!q->surface());
    surface = new WSurface(m_handle->base->surface);
    surface->setAttachedData<WXdgPopupSurface>(q);

    connect();
}

void WXdgPopupSurfacePrivate::connect()
{
    W_Q(WXdgPopupSurface);
    q->listeners()->add(&m_handle->events.reposition, q, &WXdgPopupSurface::reposition);
}

WXdgPopupSurface::WXdgPopupSurface(wlr_xdg_popup *handle)
    : WXdgSurface(*new WXdgPopupSurfacePrivate(this, handle))
{
    d_func()->init();
}

WXdgPopupSurface::~WXdgPopupSurface()
{
    teardown();
    // Notify listeners while the object is still usable (its members are
    // alive during the destructor body).
    Q_EMIT beforeDestroy();
    // Owner rule: this object created the WSurface wrapper, release it.
    W_D(WXdgPopupSurface);
    // Clear the reverse fromHandle() mapping. The shell destroys this
    // wrapper from the popup's destroy callback (or while the native handle
    // is still alive), so the handle is valid here.
    if (d->m_handle && d->m_handle->base->data == this)
        d->m_handle->base->data = nullptr;
    delete d->surface;
}

bool WXdgPopupSurface::hasCapability(Capability cap) const
{
    switch (cap) {
        using enum Capability;
    case Resize:
    case Focus:
        return true;
    case Activate:
    case Maximized:
    case FullScreen:
        return false;
    default:
        break;
    }
    Q_UNREACHABLE();
}

WSurface *WXdgPopupSurface::surface() const
{
    W_DC(WXdgPopupSurface);
    return d->surface;
}

wlr_xdg_popup *WXdgPopupSurface::handle() const
{
    W_DC(WXdgPopupSurface);
    return d->handle();
}

wlr_surface *WXdgPopupSurface::inputTargetAt(QPointF &localPos) const
{
    // find a wlr_suface object who can receive the events
    const QPointF pos = localPos;
    auto *xdgSurface = handle()->base;
    auto sur = wlr_xdg_surface_surface_at(xdgSurface, pos.x(), pos.y(), &localPos.rx(), &localPos.ry());
    return sur;
}

WXdgPopupSurface *WXdgPopupSurface::fromHandle(wlr_xdg_popup *handle)
{
    if (!handle)
        return nullptr;
    return static_cast<WXdgPopupSurface*>(handle->base->data);
}

WXdgPopupSurface *WXdgPopupSurface::fromSurface(WSurface *surface)
{
    return surface->getAttachedData<WXdgPopupSurface>();
}

void WXdgPopupSurface::resize([[maybe_unused]] const QSize &size)
{

}

void WXdgPopupSurface::close()
{
    // wlr_xdg_popup_destroy will send popup_done to the client
    wlr_xdg_popup_destroy(handle());
}

QRect WXdgPopupSurface::getContentGeometry() const
{
    auto *xdgSurface = handle()->base;
    const wlr_box &tmp = xdgSurface->geometry;
    return QRect(tmp.x, tmp.y, tmp.width, tmp.height);
}

bool WXdgPopupSurface::checkNewSize(const QSize &size, [[maybe_unused]] QSize *clipedSize)
{
    return size.isValid();
}

bool WXdgPopupSurface::isInitialized() const
{
    W_DC(WXdgPopupSurface);
    return d->handle()->base->initialized;
}

WSurface *WXdgPopupSurface::parentSurface() const
{
    W_DC(WXdgPopupSurface);
    auto parent = d->handle()->parent;
    Q_ASSERT(parent);
    return WSurface::fromHandle(parent);
}

QPointF WXdgPopupSurface::getPopupPosition() const
{
    auto wpopup = handle();
    Q_ASSERT(wpopup);
    // Only query the popup geometry when the parent is another xdg popup
    // (wlr_xdg_popup_get_position asserts the parent is a valid xdg surface);
    // otherwise fall back to the committed geometry, as in master.
    if (wpopup->parent && wlr_xdg_surface_try_from_wlr_surface(wpopup->parent)) {
        double popup_sx, popup_sy;
        wlr_xdg_popup_get_position(wpopup, &popup_sx, &popup_sy);
        return QPointF(popup_sx, popup_sy);
    }
    return {static_cast<qreal>(wpopup->current.geometry.x),
            static_cast<qreal>(wpopup->current.geometry.y)};
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wxdgpopupsurface.cpp"

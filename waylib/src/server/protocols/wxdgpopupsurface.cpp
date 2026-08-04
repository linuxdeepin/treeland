// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgpopupsurface.h"
#include "private/wtoplevelsurface_p.h"

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgPopupSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WXdgPopupSurfacePrivate(WXdgPopupSurface *qq, wlr_xdg_popup *handle);
    ~WXdgPopupSurfacePrivate();

    WWRAP_NATIVE_HANDLE_FUNCTIONS(wlr_xdg_popup)

    wl_client *waylandClient() const override {
        return handle()->base->client->client;
    }

    void init();
    void connect();

    void instantRelease() override;

    W_DECLARE_PUBLIC(WXdgPopupSurface)

    WSurface *surface = nullptr;
    QPointF position;

    WScopedListener m_repositionListener;
};

WXdgPopupSurfacePrivate::WXdgPopupSurfacePrivate(WXdgPopupSurface *qq, wlr_xdg_popup *hh)
    : WToplevelSurfacePrivate(qq)
{
    initNativeHandle(hh, &hh->events.destroy);
}

WXdgPopupSurfacePrivate::~WXdgPopupSurfacePrivate()
{

}

void WXdgPopupSurfacePrivate::instantRelease()
{
    if (!surface)
        return;
    m_repositionListener.remove();
    surface->safeDeleteLater();
    surface = nullptr;
}

void WXdgPopupSurfacePrivate::init()
{
    W_Q(WXdgPopupSurface);

    Q_ASSERT(!q->surface());
    surface = new WSurface(handle()->base->surface, q);
    surface->setAttachedData<WXdgPopupSurface>(q);

    connect();
}


void WXdgPopupSurfacePrivate::connect()
{
    W_Q(WXdgPopupSurface);
    wlr_xdg_popup *popup = handle();
    m_repositionListener.connect(&popup->events.reposition, [q](wl_listener *, void *) {
        Q_EMIT q->reposition();
    });
}

WXdgPopupSurface::WXdgPopupSurface(wlr_xdg_popup *handle, QObject *parent)
    : WXdgSurface(*new WXdgPopupSurfacePrivate(this, handle), parent)
{
    d_func()->init();
}

WXdgPopupSurface::~WXdgPopupSurface()
{

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
    // find a wlr_surface object who can receive the events
    const QPointF pos = localPos;
    auto sur = wlr_xdg_surface_surface_at(d_func()->handle()->base, pos.x(), pos.y(), &localPos.rx(), &localPos.ry());
    return sur;
}

WXdgPopupSurface *WXdgPopupSurface::fromHandle(wlr_xdg_popup *handle)
{
    return static_cast<WXdgPopupSurface*>(WWrapObjectPrivate::fromNativeHandle(handle));
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
    wlr_xdg_popup_destroy(d_func()->handle());
}

QRect WXdgPopupSurface::getContentGeometry() const
{
    auto xdgSurface = d_func()->handle()->base;
    return QRect(xdgSurface->geometry.x, xdgSurface->geometry.y,
                 xdgSurface->geometry.width, xdgSurface->geometry.height);
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
    auto wpopup = d_func()->handle();
    Q_ASSERT(wpopup);
    if (wpopup->parent && wlr_xdg_popup_try_from_wlr_surface(wpopup->parent)) {
        double popup_sx, popup_sy;
        wlr_xdg_popup_get_position(wpopup, &popup_sx, &popup_sy);
        return QPointF(popup_sx, popup_sy);
    }
    return {static_cast<qreal>(wpopup->current.geometry.x),
            static_cast<qreal>(wpopup->current.geometry.y)};
}

WAYLIB_SERVER_END_NAMESPACE

// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgpopupsurface.h"
#include "private/wtoplevelsurface_p.h"

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using PopupRegistry = QHash<const wlr_xdg_popup *, WXdgPopupSurface *>;
Q_GLOBAL_STATIC(PopupRegistry, s_popups)

class Q_DECL_HIDDEN WXdgPopupSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WXdgPopupSurfacePrivate(WXdgPopupSurface *qq, wlr_xdg_popup *handle);
    ~WXdgPopupSurfacePrivate();

    inline wlr_xdg_popup *handle() const { return popupHandle; }

    wl_client *waylandClient() const override {
        return popupHandle ? popupHandle->base->client->client : nullptr;
    }

    void init();
    void connectNativeEvents();
    void disconnectNativeEvents();

    void instantRelease() override;

    W_DECLARE_PUBLIC(WXdgPopupSurface)

    wlr_xdg_popup *popupHandle = nullptr;
    WNativeListener destroyListener;
    WNativeListener repositionListener;
    WSurface *surface = nullptr;
    QPointF position;
};

WXdgPopupSurfacePrivate::WXdgPopupSurfacePrivate(WXdgPopupSurface *qq, wlr_xdg_popup *handle)
    : WToplevelSurfacePrivate(qq)
    , popupHandle(handle)
{
    Q_ASSERT(popupHandle);
    Q_ASSERT(!s_popups->contains(popupHandle));
    s_popups->insert(popupHandle, qq);
}

WXdgPopupSurfacePrivate::~WXdgPopupSurfacePrivate()
{

}

void WXdgPopupSurfacePrivate::instantRelease()
{
    if (popupHandle) {
        disconnectNativeEvents();
        s_popups->remove(popupHandle);
        popupHandle = nullptr;
    }
    if (surface) {
        surface->safeDeleteLater();
        surface = nullptr;
    }
}

void WXdgPopupSurfacePrivate::init()
{
    W_Q(WXdgPopupSurface);
    Q_ASSERT(!q->surface());
    surface = new WSurface(handle()->base->surface, q);
    surface->setAttachedData<WXdgPopupSurface>(q);

    connectNativeEvents();
}

void WXdgPopupSurfacePrivate::connectNativeEvents()
{
    W_Q(WXdgPopupSurface);
    destroyListener.connect(&handle()->events.destroy, [q](void *) {
        q->safeDeleteLater();
    });
    repositionListener.connect(&handle()->events.reposition, [q](void *) {
        Q_EMIT q->reposition();
    });
}

void WXdgPopupSurfacePrivate::disconnectNativeEvents()
{
    destroyListener.disconnect();
    repositionListener.disconnect();
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
    // find a wlr_suface object who can receive the events
    const QPointF pos = localPos;
    return wlr_xdg_surface_surface_at(handle()->base, pos.x(), pos.y(),
                                      &localPos.rx(), &localPos.ry());
}

WXdgPopupSurface *WXdgPopupSurface::fromHandle(wlr_xdg_popup *handle)
{
    return s_popups->value(handle);
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
    wlr_xdg_popup_destroy(handle());
}

QRect WXdgPopupSurface::getContentGeometry() const
{
    const auto &geometry = handle()->base->geometry;
    return QRect(geometry.x, geometry.y, geometry.width, geometry.height);
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
    if (wpopup->parent && wlr_xdg_popup_try_from_wlr_surface(wpopup->parent)) {
        double popup_sx, popup_sy;
        wlr_xdg_popup_get_position(wpopup, &popup_sx, &popup_sy);
        return QPointF(popup_sx, popup_sy);
    }
    return {static_cast<qreal>(wpopup->current.geometry.x),
            static_cast<qreal>(wpopup->current.geometry.y)};
}

WAYLIB_SERVER_END_NAMESPACE

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgpopupsurface.h"
#include "private/wtoplevelsurface_p.h"

#include <qwxdgshell.h>
#include <qwcompositor.h>
#include <qwbox.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgPopupSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WXdgPopupSurfacePrivate(WXdgPopupSurface *qq, qw_xdg_popup *handle);
    ~WXdgPopupSurfacePrivate();

    WWRAP_HANDLE_FUNCTIONS(qw_xdg_popup, wlr_xdg_popup)

    wl_client *waylandClient() const override {
        return nativeHandle()->base->client->client;
    }

    void init();
    void connect();

    void instantRelease() override;

    W_DECLARE_PUBLIC(WXdgPopupSurface)

    WSurface *surface = nullptr;
    QPointF position;
};

WXdgPopupSurfacePrivate::WXdgPopupSurfacePrivate(WXdgPopupSurface *qq, qw_xdg_popup *hh)
    : WToplevelSurfacePrivate(qq)
{
    initHandle(hh);
}

WXdgPopupSurfacePrivate::~WXdgPopupSurfacePrivate()
{

}

void WXdgPopupSurfacePrivate::instantRelease()
{
    if (!surface)
        return;
    W_Q(WXdgPopupSurface);
    handle()->set_data(nullptr, nullptr);
    handle()->disconnect(q);
    surface->safeDeleteLater();
    surface = nullptr;
}

void WXdgPopupSurfacePrivate::init()
{
    W_Q(WXdgPopupSurface);
    handle()->set_data(this, q);

    Q_ASSERT(!q->surface());
    surface = new WSurface(qw_surface::from(nativeHandle()->base->surface), q);
    surface->setAttachedData<WXdgPopupSurface>(q);

    connect();
}


void WXdgPopupSurfacePrivate::connect()
{
    W_Q(WXdgPopupSurface);
    QObject::connect(handle(), &qw_xdg_popup::notify_reposition, q, &WXdgPopupSurface::reposition);
}

WXdgPopupSurface::WXdgPopupSurface(qw_xdg_popup *handle, QObject *parent)
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
        return true;
    case Focus:
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

qw_xdg_popup *WXdgPopupSurface::handle() const
{
    W_DC(WXdgPopupSurface);
    return d->handle();
}

qw_surface *WXdgPopupSurface::inputTargetAt(QPointF &localPos) const
{
    // find a wlr_suface object who can receive the events
    const QPointF pos = localPos;
    auto xdgSurface = qw_xdg_surface::from(handle()->handle()->base);
    auto sur = xdgSurface->surface_at(pos.x(), pos.y(), &localPos.rx(), &localPos.ry());
    return sur ? qw_surface::from(sur) : nullptr;
}

WXdgPopupSurface *WXdgPopupSurface::fromHandle(qw_xdg_popup *handle)
{
    return handle->get_data<WXdgPopupSurface>();
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
    // can't use delete qw_xdg_popup, which is not owner of wlr_xdg_popup
    handle()->destroy();
}

QRect WXdgPopupSurface::getContentGeometry() const
{
    W_DC(WXdgPopupSurface);
    auto xdgSurface = qw_xdg_surface::from(handle()->handle()->base);
    qw_box tmp = qw_box(xdgSurface->handle()->geometry);
    return tmp.toQRect();
}

bool WXdgPopupSurface::checkNewSize(const QSize &size, QSize *clipedSize)
{
    Q_UNUSED(clipedSize);
    return size.isValid();
}

WSurface *WXdgPopupSurface::parentSurface() const
{
    W_DC(WXdgPopupSurface);
    auto parent = d->nativeHandle()->parent;
    Q_ASSERT(parent);
    return WSurface::fromHandle(parent);
}

QPointF WXdgPopupSurface::getPopupPosition() const
{
    auto wpopup = handle()->handle();
    Q_ASSERT(wpopup);
    if (wpopup->parent && qw_xdg_popup::try_from_wlr_surface(wpopup->parent)) {
        auto popup = qw_xdg_popup::from(wpopup);
        double popup_sx, popup_sy;
        popup->get_position(&popup_sx, &popup_sy);
        return QPointF(popup_sx, popup_sy);
    }
    return {static_cast<qreal>(wpopup->current.geometry.x),
            static_cast<qreal>(wpopup->current.geometry.y)};
}


WAYLIB_SERVER_END_NAMESPACE


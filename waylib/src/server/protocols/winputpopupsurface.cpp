// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputpopupsurface.h"

#include "private/wtoplevelsurface_p.h"
#include "wsurface.h"

#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/util/box.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WInputPopupSurfacePrivate : public WToplevelSurfacePrivate
{
public:
    W_DECLARE_PUBLIC(WInputPopupSurface)
    explicit WInputPopupSurfacePrivate(wlr_input_popup_surface_v2 *surface, WSurface *parentSurface, WInputPopupSurface *qq)
        : WToplevelSurfacePrivate(qq)
        , parent(parentSurface)
        , cursorRect()
    {
        initNativeHandle(surface, &surface->events.destroy);
    }

    WWRAP_NATIVE_HANDLE_FUNCTIONS(wlr_input_popup_surface_v2)

    QSize size() const
    {
        return {handle()->surface->current.width, handle()->surface->current.height};
    }

    wl_client *waylandClient() const override {
        return handle()->resource->client;
    }

    WSurface *const parent;
    QRect cursorRect;
};

WInputPopupSurface::WInputPopupSurface(wlr_input_popup_surface_v2 *surface, WSurface *parentSurface, QObject *parent)
    : WToplevelSurface(*new WInputPopupSurfacePrivate(surface, parentSurface, this), parent)
{ }

bool WInputPopupSurface::hasCapability(Capability cap) const
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

WSurface *WInputPopupSurface::surface() const
{
    auto wSurface = WSurface::fromHandle(d_func()->handle()->surface);
    if (!wSurface) {
        wSurface = new WSurface(d_func()->handle()->surface);
        QObject::connect(this, &WInputPopupSurface::aboutToBeInvalidated, wSurface, &WSurface::safeDeleteLater);
    }
    return wSurface;
}

wlr_input_popup_surface_v2 *WInputPopupSurface::handle() const
{
    return d_func()->handle();
}

bool WInputPopupSurface::isActivated() const
{
    return true;
}

QRect WInputPopupSurface::getContentGeometry() const
{
    return {0, 0, d_func()->size().width(), d_func()->size().height()};
}

WSurface *WInputPopupSurface::parentSurface() const
{
    return d_func()->parent;
}

bool WInputPopupSurface::checkNewSize(const QSize &, QSize *)
{
    return false;
}

QRect WInputPopupSurface::cursorRect() const
{
    return d_func()->cursorRect;
}

void WInputPopupSurface::sendCursorRect(QRect rect)
{
    W_D(WInputPopupSurface);
    if (d->cursorRect == rect)
        return;
    d->cursorRect = rect;
    wlr_box box{rect.x(), rect.y(), rect.width(), rect.height()};
    wlr_input_popup_surface_v2_send_text_input_rectangle(d->handle(), &box);

    Q_EMIT cursorRectChanged();
}
WAYLIB_SERVER_END_NAMESPACE

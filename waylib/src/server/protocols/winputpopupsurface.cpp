// Copyright (C) 2024-2026 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputpopupsurface.h"

#include "private/wtoplevelsurface_p.h"
#include "wsurface.h"

#define delete delete_c
extern "C" {
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_input_method_v2.h>
}
#undef delete

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WInputPopupSurfacePrivate : public WToplevelSurfacePrivate
{
public:
    WInputPopupSurfacePrivate(wlr_input_popup_surface_v2 *surface, WSurface *parentSurface,
                              WInputPopupSurface *qq)
        : WToplevelSurfacePrivate(qq)
        , popupHandle(surface)
        , parent(parentSurface)
    {
        Q_ASSERT(popupHandle);
    }

    void init()
    {
        W_Q(WInputPopupSurface);
        destroyListener.connect(&popupHandle->events.destroy, [q](void *) {
            q->safeDeleteLater();
        });
    }

    void instantRelease() override
    {
        destroyListener.disconnect();
        popupHandle = nullptr;
    }

    QSize size() const
    {
        if (!popupHandle || !popupHandle->surface)
            return {};
        return { popupHandle->surface->current.width, popupHandle->surface->current.height };
    }

    wl_client *waylandClient() const override
    {
        return popupHandle ? wl_resource_get_client(popupHandle->resource) : nullptr;
    }

    W_DECLARE_PUBLIC(WInputPopupSurface)

    wlr_input_popup_surface_v2 *popupHandle = nullptr;
    WNativeListener destroyListener;
    WSurface *const parent;
    QRect cursorRect;
};

WInputPopupSurface::WInputPopupSurface(wlr_input_popup_surface_v2 *surface, WSurface *parentSurface,
                                       QObject *parent)
    : WToplevelSurface(*new WInputPopupSurfacePrivate(surface, parentSurface, this), parent)
{
    d_func()->init();
}

WInputPopupSurface::~WInputPopupSurface() = default;

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
    if (!handle())
        return nullptr;
    auto *surface = WSurface::fromHandle(handle()->surface);
    if (!surface)
        surface = new WSurface(handle()->surface);
    return surface;
}

wlr_input_popup_surface_v2 *WInputPopupSurface::handle() const
{
    return d_func()->popupHandle;
}

bool WInputPopupSurface::isActivated() const
{
    return true;
}

QRect WInputPopupSurface::getContentGeometry() const
{
    return { QPoint(), d_func()->size() };
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
    if (!d->popupHandle || d->cursorRect == rect)
        return;
    d->cursorRect = rect;
    wlr_box box { rect.x(), rect.y(), rect.width(), rect.height() };
    wlr_input_popup_surface_v2_send_text_input_rectangle(d->popupHandle, &box);
    Q_EMIT cursorRectChanged();
}

WAYLIB_SERVER_END_NAMESPACE

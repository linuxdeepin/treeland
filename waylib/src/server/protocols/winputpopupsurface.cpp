// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputpopupsurface.h"
#include "wscoplistener.h"

#include "private/wtoplevelsurface_p.h"
#include "wsurface.h"

#include <wlr_all.h>

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
        Q_ASSERT(surface);
        m_handle = surface;
    }

    inline wlr_input_popup_surface_v2 *handle() const {
        return m_handle;
    }

    QSize size() const
    {
        return {m_handle->surface->current.width, m_handle->surface->current.height};
    }

    wl_client *waylandClient() const override {
        return handle()->resource->client;
    }

    WSurface *const parent;
    QRect cursorRect;

private:
    // The input-method owner destroys this handle after notifying the
    // wrapper. Keep the address stable through that callback.
    wlr_input_popup_surface_v2 *m_handle = nullptr;
    // Wrapper created on demand by surface(); released by the popup (owner
    // rule) when the native surface dies. QPointer so the destructor never
    // double-frees.
    QPointer<WSurface> surface;
};

WInputPopupSurface::WInputPopupSurface(wlr_input_popup_surface_v2 *surface, WSurface *parentSurface)
    : WToplevelSurface(*new WInputPopupSurfacePrivate(surface, parentSurface, this))
{ }

WInputPopupSurface::~WInputPopupSurface()
{
    teardown();
    W_D(WInputPopupSurface);
    if (d->surface)
        d->surface->removeListeners(const_cast<WInputPopupSurface *>(this));
    delete d->surface;
}

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
    auto wSurface = WSurface::fromHandle(handle()->surface);
    if (!wSurface) {
        // The popup creates the surface, so it releases it when the native
        // object dies (owner rule; no self-deletion in WSurface).
        wSurface = new WSurface(handle()->surface);
        auto *d = const_cast<WInputPopupSurfacePrivate*>(d_func());
        auto *popup = const_cast<WInputPopupSurface*>(this);
        wSurface->listeners(popup)->add(&wSurface->handle()->events.destroy, popup,
            [d, popup, wSurface](void *) {
            wSurface->removeListeners(popup);
            delete d->surface;
            d->surface = nullptr;
        });
        d->surface = wSurface;
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

#include "moc_winputpopupsurface.cpp"

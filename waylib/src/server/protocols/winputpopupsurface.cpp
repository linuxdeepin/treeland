// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputpopupsurface.h"

#include "private/wtoplevelsurface_p.h"
#include "wsurface.h"
#include <qwcompositor.h>
#include <qwinputmethodv2.h>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WInputPopupSurfacePrivate : public WToplevelSurfacePrivate
{
public:
    W_DECLARE_PUBLIC(WInputPopupSurface)
    explicit WInputPopupSurfacePrivate(QWInputPopupSurfaceV2 *surface, WSurface *parentSurface, WInputPopupSurface *qq)
        : WToplevelSurfacePrivate(qq)
        , parent(parentSurface)
        , cursorRect()
    {
        initHandle(surface);
    }

    WWRAP_HANDLE_FUNCTIONS(QWInputPopupSurfaceV2, wlr_input_popup_surface_v2)

    QSize size() const
    {
        return {handle()->surface()->handle()->current.width, handle()->surface()->handle()->current.height};
    }

    WSurface *const parent;
    QRect cursorRect;
};

WInputPopupSurface::WInputPopupSurface(QWInputPopupSurfaceV2 *surface, WSurface *parentSurface, QObject *parent)
    : WToplevelSurface(*new WInputPopupSurfacePrivate(surface, parentSurface, this), parent)
{ }

bool WInputPopupSurface::doesNotAcceptFocus() const
{
    return true;
}

WSurface *WInputPopupSurface::surface() const
{
    auto wSurface = WSurface::fromHandle(handle()->surface());
    if (!wSurface) {
        wSurface = new WSurface(handle()->surface());
        connect(handle(), &QWSurface::beforeDestroy, wSurface, &WSurface::safeDeleteLater);
    }
    return wSurface;
}

QWInputPopupSurfaceV2 *WInputPopupSurface::handle() const
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

bool WInputPopupSurface::checkNewSize(const QSize &size)
{
    return size == d_func()->size();
}

QRect WInputPopupSurface::cursorRect() const
{
    return d_func()->cursorRect;
}

void WInputPopupSurface::sendCursorRect(QRect rect)
{
    W_D(WInputPopupSurface);
    d->handle()->send_text_input_rectangle(rect);
    if (d->cursorRect == rect)
        return;
    d->cursorRect = rect;
    Q_EMIT cursorRectChanged();
}
WAYLIB_SERVER_END_NAMESPACE

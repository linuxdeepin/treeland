// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapersurface.h"
#include "wallpapershellinterfacev1.h"
#include "private/wtoplevelsurface_p.h"
#include "private/wglobal_p.h"
#include <wscoplistener.h>

class Q_DECL_HIDDEN WallpaperSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WallpaperSurfacePrivate(WallpaperSurface *qq, TreelandWallpaperSurfaceInterfaceV1 *surface);

    wl_client *waylandClient() const override {
        return interface->client();
    }

    WSurface *surface = nullptr;
    TreelandWallpaperSurfaceInterfaceV1 *interface = nullptr;
};

WallpaperSurfacePrivate::WallpaperSurfacePrivate(WallpaperSurface *qq, TreelandWallpaperSurfaceInterfaceV1 *surface)
    : WToplevelSurfacePrivate(qq)
    , interface(surface)
{
}

WallpaperSurface::WallpaperSurface(TreelandWallpaperSurfaceInterfaceV1 *handle)
    : WToplevelSurface(*new WallpaperSurfacePrivate(this, handle))
{
    init();
}

WallpaperSurface::~WallpaperSurface()
{
    // Owner rule: this object created the WSurface wrapper, release it.
    W_D(WallpaperSurface);
    delete d->surface;
}

WSurface *WallpaperSurface::surface() const
{
    W_DC(WallpaperSurface);
    return d->surface;
}

QRect WallpaperSurface::getContentGeometry() const
{
    W_DC(WallpaperSurface);
    wlr_surface *surface = wlr_surface_from_resource(d->interface->surfaceResource());
    if (!surface) {
        return QRect();
    }

    return QRect(0, 0, surface->current.width, surface->current.height);
}

bool WallpaperSurface::checkNewSize(const QSize &size, QSize *clipedSize)
{
    if (size.width() < 0 || size.height() < 0) {
        if (clipedSize)
            *clipedSize = QSize(0, 0);
        return false;
    }

    return true;
}

void WallpaperSurface::init()
{
    W_D(WallpaperSurface);

    auto *wlrSurface = wlr_surface_from_resource(d->interface->surfaceResource());
    if (!wlrSurface) {
        wl_resource_post_error(d->interface->surfaceResource(),
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid wl_surface");
        return;
    }

    d->surface = new WSurface(wlrSurface);
    d->surface->setAttachedData<WallpaperSurface>(this);

    // The client may destroy the wl_surface while the protocol object is
    // still alive. Release the WSurface wrapper as soon as the native
    // surface is gone so nothing keeps a dangling handle; wlr_surface_destroy
    // asserts the destroy listener list is empty, so detach first.
    d->surface->listeners(this)->add(&wlrSurface->events.destroy, this, [this, d]() {
        d->surface->removeListeners(this);
        delete d->surface;
        d->surface = nullptr;
        Q_EMIT surfaceChanged();
    });
}

#include "moc_wallpapersurface.cpp"

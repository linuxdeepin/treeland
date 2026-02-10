// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapersurface.h"
#include "wallpapershellinterfacev1.h"
#include "private/wtoplevelsurface_p.h"
#include "private/wglobal_p.h"

#include <qwcompositor.h>

class Q_DECL_HIDDEN WallpaperSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WallpaperSurfacePrivate(WallpaperSurface *qq, TreelandWallpaperSurfaceInterfaceV1 *surface);
    ~WallpaperSurfacePrivate();

    wl_client *waylandClient() const override {
        return interface->client();
    }
    void instantRelease() override {
    }

    WSurface *surface = nullptr;
    TreelandWallpaperSurfaceInterfaceV1 *interface = nullptr;
};

WallpaperSurfacePrivate::WallpaperSurfacePrivate(WallpaperSurface *qq, TreelandWallpaperSurfaceInterfaceV1 *surface)
    : WToplevelSurfacePrivate(qq)
    , interface(surface)
{
}

WallpaperSurfacePrivate::~WallpaperSurfacePrivate()
{
}

WallpaperSurface::WallpaperSurface(TreelandWallpaperSurfaceInterfaceV1 *handle, QObject *parent)
    : WToplevelSurface(*new WallpaperSurfacePrivate(this, handle), parent)
{
    init();
}

WallpaperSurface::~WallpaperSurface() = default;

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

    d->surface = new WSurface(qw_surface::from(wlr_surface_from_resource(d->interface->surfaceResource())), this);
    d->surface->setAttachedData<WallpaperSurface>(this);
}

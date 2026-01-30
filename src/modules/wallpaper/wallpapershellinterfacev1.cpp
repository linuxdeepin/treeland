// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapershellinterfacev1.h"
#include "qwayland-server-treeland-wallpaper-shell-unstable-v1.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwseat.h>

static QList<TreelandWallpaperSurfaceInterfaceV1 *> s_wallpaperSurfaces;

class TreelandWallpaperShellInterfaceV1Private : public QtWaylandServer::treeland_wallpaper_shell_v1
{
public:
    TreelandWallpaperShellInterfaceV1Private(TreelandWallpaperShellInterfaceV1 *_q);
    wl_global *global() const;

    TreelandWallpaperShellInterfaceV1 *q = nullptr;

protected:
    void treeland_wallpaper_shell_v1_destroy_global() override;
    void treeland_wallpaper_shell_v1_destroy(Resource *resource) override;
    void treeland_wallpaper_shell_v1_get_treeland_wallpaper_surface(Resource *resource,
                                                                    uint32_t id,
                                                                    struct ::wl_resource *surface,
                                                                    const QString &file_source) override;
};

TreelandWallpaperShellInterfaceV1Private::TreelandWallpaperShellInterfaceV1Private(TreelandWallpaperShellInterfaceV1 *_q)
    : q(_q)
{
}

wl_global *TreelandWallpaperShellInterfaceV1Private::global() const
{
    return m_global;
}

void TreelandWallpaperShellInterfaceV1Private::treeland_wallpaper_shell_v1_destroy_global()
{
    delete q;
}

void TreelandWallpaperShellInterfaceV1Private::treeland_wallpaper_shell_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TreelandWallpaperShellInterfaceV1Private::treeland_wallpaper_shell_v1_get_treeland_wallpaper_surface(Resource *resource,
                                                                                                              uint32_t id,
                                                                                                              wl_resource *surface,
                                                                                                              const QString &file_source)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "surface resource is NULL!");
        return;
    }

    wl_resource *surfaceResource = wl_resource_create(resource->client(),
                                                      &treeland_wallpaper_surface_v1_interface,
                                                      resource->version(),
                                                      id);
    if (!surfaceResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto wallpaperSurface = new TreelandWallpaperSurfaceInterfaceV1(surface, file_source, surfaceResource);
    s_wallpaperSurfaces.append(wallpaperSurface);

    QObject::connect(wallpaperSurface, &QObject::destroyed, [wallpaperSurface]() {
        s_wallpaperSurfaces.removeOne(wallpaperSurface);
    });

    Q_EMIT q->wallpaperSurfaceAdded(wallpaperSurface);
}

TreelandWallpaperShellInterfaceV1::TreelandWallpaperShellInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new TreelandWallpaperShellInterfaceV1Private(this))
{
}

TreelandWallpaperShellInterfaceV1::~TreelandWallpaperShellInterfaceV1() = default;

void TreelandWallpaperShellInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void TreelandWallpaperShellInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d = nullptr;
}

wl_global *TreelandWallpaperShellInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView TreelandWallpaperShellInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

class TreelandWallpaperSurfaceInterfaceV1Private : public QtWaylandServer::treeland_wallpaper_surface_v1
{
public:
    TreelandWallpaperSurfaceInterfaceV1Private(TreelandWallpaperSurfaceInterfaceV1 *_q,
                                               const QString &source,
                                               wl_resource *surface,
                                               wl_resource *_resource);

    TreelandWallpaperSurfaceInterfaceV1 *q;
    wl_resource *surfaceResource = nullptr;
    wl_resource *resource = nullptr;
    QString wallpaperSource;

protected:
    void treeland_wallpaper_surface_v1_destroy_resource(Resource *resource) override;
    void treeland_wallpaper_surface_v1_destroy(Resource *resource) override;
    void treeland_wallpaper_surface_v1_source_failed(Resource *resource,
                                                     uint32_t error) override;
};

TreelandWallpaperSurfaceInterfaceV1Private::TreelandWallpaperSurfaceInterfaceV1Private(TreelandWallpaperSurfaceInterfaceV1 *_q,
                                                                                       const QString &source,
                                                                                       wl_resource *surface,
                                                                                       wl_resource *_resource)
    : QtWaylandServer::treeland_wallpaper_surface_v1(_resource)
    , q(_q)
    , surfaceResource(surface)
    , resource(_resource)
    , wallpaperSource(source)
{
}

void TreelandWallpaperSurfaceInterfaceV1Private::treeland_wallpaper_surface_v1_destroy_resource([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->beforeDestroy(q);
    delete q;
}

void TreelandWallpaperSurfaceInterfaceV1Private::treeland_wallpaper_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TreelandWallpaperSurfaceInterfaceV1Private::treeland_wallpaper_surface_v1_source_failed([[maybe_unused]] Resource *resource,
                                                                                             uint32_t error)
{
    Q_EMIT q->failed(error);
}

TreelandWallpaperSurfaceInterfaceV1::~TreelandWallpaperSurfaceInterfaceV1() = default;

WSurface *TreelandWallpaperSurfaceInterfaceV1::wSurface() const
{
    return WSurface::fromHandle(qw_surface::from(wlr_surface_from_resource(d->surfaceResource)));
}

QString TreelandWallpaperSurfaceInterfaceV1::source() const
{
    return d->wallpaperSource;
}

wl_client *TreelandWallpaperSurfaceInterfaceV1::client()
{
    return d->resource->client;
}

wl_resource *TreelandWallpaperSurfaceInterfaceV1::surfaceResource()
{
    return d->surfaceResource;
}

TreelandWallpaperSurfaceInterfaceV1 *TreelandWallpaperSurfaceInterfaceV1::get(WSurface *surface)
{
    for (TreelandWallpaperSurfaceInterfaceV1 *surfaceInterface : std::as_const(s_wallpaperSurfaces)) {
        if (surfaceInterface->wSurface() == surface) {
            return surfaceInterface;
        }
    }

    return nullptr;
}

TreelandWallpaperSurfaceInterfaceV1 *TreelandWallpaperSurfaceInterfaceV1::get(const QString &source)
{
    for (TreelandWallpaperSurfaceInterfaceV1 *surfaceInterface : std::as_const(s_wallpaperSurfaces)) {
        if (surfaceInterface->source() == source) {
            return surfaceInterface;
        }
    }

    return nullptr;
}

TreelandWallpaperSurfaceInterfaceV1::TreelandWallpaperSurfaceInterfaceV1(wl_resource *surface,
                                                                         const QString &source,
                                                                         wl_resource *resource)
    : d(new TreelandWallpaperSurfaceInterfaceV1Private(this, source, surface, resource))
{
}

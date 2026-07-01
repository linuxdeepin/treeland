// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapernotifierinterfacev1.h"
#include "qwayland-server-treeland-wallpaper-shell-unstable-v1.h"

#include <qwdisplay.h>

class TreelandWallpaperNotifierInterfaceV1Private : public QtWaylandServer::treeland_wallpaper_notifier_v1
{
public:
    TreelandWallpaperNotifierInterfaceV1Private(TreelandWallpaperNotifierInterfaceV1 *_q);
    wl_global *global() const;

    TreelandWallpaperNotifierInterfaceV1 *q = nullptr;

protected:
    void bind_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
};

TreelandWallpaperNotifierInterfaceV1Private::TreelandWallpaperNotifierInterfaceV1Private(TreelandWallpaperNotifierInterfaceV1 *_q)
    : QtWaylandServer::treeland_wallpaper_notifier_v1()
    , q(_q)
{
}

wl_global *TreelandWallpaperNotifierInterfaceV1Private::global() const
{
    return m_global;
}

void TreelandWallpaperNotifierInterfaceV1Private::bind_resource(Resource *resource)
{
    Q_EMIT q->bound(resource->handle);
}

void TreelandWallpaperNotifierInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

TreelandWallpaperNotifierInterfaceV1::TreelandWallpaperNotifierInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new TreelandWallpaperNotifierInterfaceV1Private(this))
{
}

void TreelandWallpaperNotifierInterfaceV1::sendAdd(TreelandWallpaperInterfaceV1::WallpaperType type, const QString &fileSource)
{
    for (const auto &resource : d->resourceMap()) {
        d->send_add(resource->handle, type, fileSource);
    }
}

void TreelandWallpaperNotifierInterfaceV1::sendRemove(const QString &fileSource)
{
    for (const auto &resource : d->resourceMap()) {
        d->send_remove(resource->handle, fileSource);
    }
}

TreelandWallpaperNotifierInterfaceV1::~TreelandWallpaperNotifierInterfaceV1() = default;

void TreelandWallpaperNotifierInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void TreelandWallpaperNotifierInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *TreelandWallpaperNotifierInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView TreelandWallpaperNotifierInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void TreelandWallpaperNotifierInterfaceV1::sendAddForResource(wl_resource *resource, TreelandWallpaperInterfaceV1::WallpaperType type, const QString &fileSource)
{
    d->send_add(resource, type, fileSource);
}

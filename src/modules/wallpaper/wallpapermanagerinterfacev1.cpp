// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapermanagerinterfacev1.h"
#include "qwayland-server-treeland-wallpaper-manager-unstable-v1.h"
#include "workspace.h"
#include "helper.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include <woutput.h>
#include <wsocket.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwseat.h>

static QList<TreelandWallpaperInterfaceV1 *> s_wallpapers;

static const char *usernameFromUid(uid_t uid)
{
    static char buf[16384];
    static const size_t bufSize = sizeof(buf);
    static struct passwd pwd;
    struct passwd *result = NULL;

    int ret = getpwuid_r(uid, &pwd, buf, bufSize, &result);
    if (ret != 0 || result == NULL)
        return NULL;

    return pwd.pw_name;
}

class TreelandWallpaperManagerInterfaceV1Private : public QtWaylandServer::treeland_wallpaper_manager_v1
{
public:
    explicit TreelandWallpaperManagerInterfaceV1Private(TreelandWallpaperManagerInterfaceV1 *_q);
    wl_global *global() const;

    TreelandWallpaperManagerInterfaceV1 *q = nullptr;
protected:
    void treeland_wallpaper_manager_v1_destroy_global() override;
    void treeland_wallpaper_manager_v1_destroy(Resource *resource) override;
    void treeland_wallpaper_manager_v1_get_treeland_wallpaper(Resource *resource,
                                                              uint32_t id,
                                                              struct ::wl_resource *output,
                                                              struct ::wl_resource *surface) override;

};

TreelandWallpaperManagerInterfaceV1Private::TreelandWallpaperManagerInterfaceV1Private(TreelandWallpaperManagerInterfaceV1 *_q)
    : q(_q)
{
}

wl_global *TreelandWallpaperManagerInterfaceV1Private::global() const
{
    return m_global;
}

void TreelandWallpaperManagerInterfaceV1Private::treeland_wallpaper_manager_v1_destroy_global()
{
    delete q;
}

void TreelandWallpaperManagerInterfaceV1Private::treeland_wallpaper_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TreelandWallpaperManagerInterfaceV1Private::treeland_wallpaper_manager_v1_get_treeland_wallpaper(Resource *resource,
                                                                                                      uint32_t id,
                                                                                                      struct ::wl_resource *output,
                                                                                                      struct ::wl_resource *surface)
{
    if (!output) {
        wl_resource_post_error(resource->handle, 0, "output resource is NULL!");
        return;
    }

    wl_resource *wallpaperResource = wl_resource_create(resource->client(),
                                                     &treeland_wallpaper_v1_interface,
                                                     resource->version(),
                                                     id);
    if (!wallpaperResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    QSharedPointer<WClient::Credentials>  credentials = WClient::getCredentials(wallpaperResource->client);
    auto wallpaper = new TreelandWallpaperInterfaceV1(output, QString(usernameFromUid(credentials->uid)), wallpaperResource, surface);
    s_wallpapers.append(wallpaper);

    QObject::connect(wallpaper, &QObject::destroyed, [wallpaper]() {
        s_wallpapers.removeOne(wallpaper);
    });

    Q_EMIT q->wallpaperCreated(wallpaper);
}

TreelandWallpaperManagerInterfaceV1::TreelandWallpaperManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new TreelandWallpaperManagerInterfaceV1Private(this))
{
}

TreelandWallpaperManagerInterfaceV1::~TreelandWallpaperManagerInterfaceV1() = default;

void TreelandWallpaperManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void TreelandWallpaperManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d = nullptr;
}

wl_global *TreelandWallpaperManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView TreelandWallpaperManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

class TreelandWallpaperInterfaceV1Private : public QtWaylandServer::treeland_wallpaper_v1
{
public:
    TreelandWallpaperInterfaceV1Private(TreelandWallpaperInterfaceV1 *_q,
                                    wl_resource *output,
                                    const QString &name,
                                    wl_resource *_resource,
                                    wl_resource *refsurface);

    TreelandWallpaperInterfaceV1 *q = nullptr;
    wl_resource *outputResource = nullptr;
    QString userName;
    wl_resource *resource = nullptr;
    wl_resource *refSurfaceResource = nullptr;

protected:
    void treeland_wallpaper_v1_bind_resource(Resource *resource) override;
    void treeland_wallpaper_v1_destroy_resource(Resource *resource) override;
    void treeland_wallpaper_v1_destroy(Resource *resource) override;
    void treeland_wallpaper_v1_set_image_source(Resource *resource, const QString &fileSource, uint32_t nativeRole) override;
    void treeland_wallpaper_v1_set_video_source(Resource *resource, const QString &fileSource, uint32_t nativeRole) override;
};

TreelandWallpaperInterfaceV1Private::TreelandWallpaperInterfaceV1Private(TreelandWallpaperInterfaceV1 *_q,
                                                                         wl_resource *output,
                                                                         const QString &name,
                                                                         wl_resource *_resource,
                                                                         wl_resource *refsurface)
    : QtWaylandServer::treeland_wallpaper_v1(_resource)
    , q(_q)
    , outputResource(output)
    , userName(name)
    , resource(_resource)
    , refSurfaceResource(refsurface)
{
}

void TreelandWallpaperInterfaceV1Private::treeland_wallpaper_v1_bind_resource([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->binded();
}

void TreelandWallpaperInterfaceV1Private::treeland_wallpaper_v1_destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void TreelandWallpaperInterfaceV1Private::treeland_wallpaper_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TreelandWallpaperInterfaceV1Private::treeland_wallpaper_v1_set_image_source([[maybe_unused]] Resource *resource,
                                                                                 const QString &fileSource,
                                                                                 uint32_t nativeRole)
{
    TreelandWallpaperInterfaceV1::WallpaperRoles roles =
        static_cast<TreelandWallpaperInterfaceV1::WallpaperRoles>(nativeRole);
    Workspace *workspace = Helper::instance()->workspace();
    Q_ASSERT(workspace);
    Q_EMIT q->imageSourceChanged(workspace->currentIndex(), fileSource, roles);
}

void TreelandWallpaperInterfaceV1Private::treeland_wallpaper_v1_set_video_source([[maybe_unused]] Resource *resource,
                                                                                 const QString &fileSource,
                                                                                 uint32_t nativeRole)
{
    TreelandWallpaperInterfaceV1::WallpaperRoles roles =
        static_cast<TreelandWallpaperInterfaceV1::WallpaperRoles>(nativeRole);
    Workspace *workspace = Helper::instance()->workspace();
    Q_ASSERT(workspace);
    Q_EMIT q->videoSourceChanged(workspace->currentIndex(), fileSource, roles);
}

TreelandWallpaperInterfaceV1::TreelandWallpaperInterfaceV1(wl_resource *output,
                                                           const QString &userName,
                                                           wl_resource *resource,
                                                           wl_resource *refsurface,
                                                           QObject *parent)
    : QObject(parent)
    , d(new TreelandWallpaperInterfaceV1Private(this, output, userName, resource, refsurface))
{
}

TreelandWallpaperInterfaceV1::~TreelandWallpaperInterfaceV1() = default;

WSurface *TreelandWallpaperInterfaceV1::referenceWSurface() const
{
    if (!d->refSurfaceResource) {
        return nullptr;
    }

    return WSurface::fromHandle(qw_surface::from(wlr_surface_from_resource(d->refSurfaceResource)));
}

QString TreelandWallpaperInterfaceV1::userName() const
{
    return d->userName;
}

WOutput *TreelandWallpaperInterfaceV1::wOutput() const
{
    return WOutput::fromHandle(qw_output::from(wlr_output_from_resource(d->outputResource)));
}

void TreelandWallpaperInterfaceV1::sendError(const QString &source, Error error)
{
    d->send_failed(source, error);
}

void TreelandWallpaperInterfaceV1::sendChanged(WallpaperRoles roles, WallpaperType type, const QString &source)
{
    d->send_changed(roles, type, source);
}

TreelandWallpaperInterfaceV1 *TreelandWallpaperInterfaceV1::getReferenceWallpaperInterfaceFromSurface(WSurface *surface)
{
    for (TreelandWallpaperInterfaceV1 *wallpaperInterface : std::as_const(s_wallpapers)) {
        if (wallpaperInterface->referenceWSurface() == surface) {
            return wallpaperInterface;
        }
    }

    return nullptr;
}

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapercolorinterfacev1.h"
#include "qwayland-server-treeland-wallpaper-color-v1.h"

#include <woutput.h>
#include <wserver.h>

#include <qwdisplay.h>

#include <QMap>
#include <QDebug>
#include <QQmlInfo>
#include <QString>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(qlcWallpapercolor, "treeland.modules.wallpapercolor", QtWarningMsg)

class WallpaperColorInterfaceV1Private : public QtWaylandServer::treeland_wallpaper_color_manager_v1
{
public:
    WallpaperColorInterfaceV1Private(WallpaperColorInterfaceV1 *_q);
    wl_global *global() const;

    WallpaperColorInterfaceV1 *q;
    QMap<wl_resource *, QList<QString>> watch_lists;
    QMap<QString, bool> color_map;
    QList<Resource *> m_resource;

protected:
    void bind_resource(Resource *resource) override;
    void destroy_resource(Resource *resource) override;
    void destroy_global() override;
    void destroy(Resource *resource) override;
    void watch(Resource *resource, const QString &output) override;
    void unwatch(Resource *resource, const QString &output) override;
};

WallpaperColorInterfaceV1Private::WallpaperColorInterfaceV1Private(WallpaperColorInterfaceV1 *_q)
    : q(_q)
{
}

wl_global *WallpaperColorInterfaceV1Private::global() const
{
    return m_global;
}

void WallpaperColorInterfaceV1Private::bind_resource(Resource *resource)
{
    m_resource.append(resource);
}

void WallpaperColorInterfaceV1Private::destroy_resource(Resource *resource)
{
    m_resource.removeOne(resource);
    watch_lists.remove(resource->handle);
}

void WallpaperColorInterfaceV1Private::destroy_global() {
    m_resource.clear();
    delete q;
}

void WallpaperColorInterfaceV1Private::destroy(Resource *resource) {
    wl_resource_destroy(resource->handle);
}

void WallpaperColorInterfaceV1Private::watch(Resource *resource, const QString &output)
{
    if (!color_map.contains(output)) {
        qCWarning(qlcWallpapercolor)
        << QString("Wallpaper info for output:(%1) never set, will ignore "
                   "this requset!")
                .arg(output);
        return;
    }

    auto isDark = color_map[output];
    qCDebug(qlcWallpapercolor) << QString("New watch requset for output:(%1), it's wallpaper is %2")
                                      .arg(output, isDark ? "dark" : "light");
    send_output_color(resource->handle, output, isDark);
    watch_lists[resource->handle].append(output);
}

void WallpaperColorInterfaceV1Private::unwatch(Resource *resource, const QString &output)
{
    watch_lists[resource->handle].removeOne(output);
}

WallpaperColorInterfaceV1::WallpaperColorInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new WallpaperColorInterfaceV1Private(this))
{
}

WallpaperColorInterfaceV1::~WallpaperColorInterfaceV1() = default;

void WallpaperColorInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void WallpaperColorInterfaceV1::destroy([[maybe_unused]] WServer *server) {
    d = nullptr;
}

wl_global *WallpaperColorInterfaceV1::global() const
{
    return d->global();
}

void WallpaperColorInterfaceV1::updateWallpaperColor(const QString &output, bool isDarkType)
{
    if (d->color_map.contains(output)) {
        if (d->color_map[output] == isDarkType)
            return;
    }
    qCDebug(qlcWallpapercolor)
        << QString("Wallpaper info for output:(%1) changed, it's wallpaper is %2")
               .arg(output, isDarkType ? "dark" : "light");

    d->color_map[output] = isDarkType;
    for (auto i = d->watch_lists.cbegin(), end = d->watch_lists.cend(); i != end; ++i)
        if (i.value().contains(output))
            d->send_output_color(i.key(),
                                 output,
                                 isDarkType);
}

QByteArrayView WallpaperColorInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "screensaverinterfacev2.h"
#include "qwayland-server-treeland-screensaver-unstable-v2.h"
#include "helper.h"

#include <wayland-server.h>
#include <wayland-util.h>

#include <QDebug>

class ScreensaverInterfaceV2Private : public QtWaylandServer::treeland_screensaver_v2
{
public:
    explicit ScreensaverInterfaceV2Private(ScreensaverInterfaceV2 *_q);
    wl_global *global() const;

    ScreensaverInterfaceV2 *q = nullptr;
    QHash<wl_resource*, std::tuple<QString, QString>> inhibits;
protected:
    void inhibit(Resource *resource, const QString &application_name, const QString &reason_for_inhibit) override;
    void uninhibit(Resource *resource) override;
    void destroy(Resource *resource) override;
};

ScreensaverInterfaceV2Private::ScreensaverInterfaceV2Private(ScreensaverInterfaceV2 *_q)
    : QtWaylandServer::treeland_screensaver_v2()
    , q(_q)
{
}

wl_global *ScreensaverInterfaceV2Private::global() const
{
    return m_global;
}

void ScreensaverInterfaceV2Private::destroy(Resource *resource)
{
    uninhibit(resource);
    wl_resource_destroy(resource->handle);
}

void ScreensaverInterfaceV2Private::inhibit(Resource *resource, const QString &application_name, const QString &reason_for_inhibit)
{
    wl_resource *res = resource->handle;
    if (inhibits.contains(res)) {
        wl_resource_post_error(res, TREELAND_SCREENSAVER_V2_ERROR_ALREADY_INHIBITED,
                               "Trying to inhibit with an existing inhibit active");
        return;
    }

    inhibits.insert(res, std::make_tuple(application_name, reason_for_inhibit));
    Helper::instance()->updateIdleInhibitor();
}

void ScreensaverInterfaceV2Private::uninhibit(Resource *resource)
{
    wl_resource *res = resource->handle;
    if (!inhibits.contains(res)) {
        wl_resource_post_error(res, TREELAND_SCREENSAVER_V2_ERROR_NOT_YET_INHIBITED,
                               "Trying to uninhibit but no active inhibit existed");
        return;
    }

    inhibits.remove(res);
    Helper::instance()->updateIdleInhibitor();
}

ScreensaverInterfaceV2::ScreensaverInterfaceV2(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new ScreensaverInterfaceV2Private(this))
{
}

ScreensaverInterfaceV2::~ScreensaverInterfaceV2() = default;

QByteArrayView ScreensaverInterfaceV2::interfaceName() const
{
    return d->interfaceName();
}

bool ScreensaverInterfaceV2::isInhibited() const
{
    return !d->inhibits.isEmpty();
}

void ScreensaverInterfaceV2::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void ScreensaverInterfaceV2::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *ScreensaverInterfaceV2::global() const
{
    return d->global();
}

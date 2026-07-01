// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "screensaverinterfacev1.h"
#include "qwayland-server-treeland-screensaver-v1.h"
#include "helper.h"

#include <wayland-server.h>
#include <wayland-util.h>

#include <qwdisplay.h>

#include <QDebug>

class ScreensaverInterfaceV1Private : public QtWaylandServer::treeland_screensaver_v1
{
public:
    explicit ScreensaverInterfaceV1Private(ScreensaverInterfaceV1 *_q);
    wl_global *global() const;

    ScreensaverInterfaceV1 *q = nullptr;
    QHash<wl_resource*, std::tuple<QString, QString>> inhibits;
protected:
    void inhibit(Resource *resource, const QString &application_name, const QString &reason_for_inhibit) override;
    void uninhibit(Resource *resource) override;
    void destroy(Resource *resource) override;
};

ScreensaverInterfaceV1Private::ScreensaverInterfaceV1Private(ScreensaverInterfaceV1 *_q)
    : QtWaylandServer::treeland_screensaver_v1()
    , q(_q)
{
}

wl_global *ScreensaverInterfaceV1Private::global() const
{
    return m_global;
}

void ScreensaverInterfaceV1Private::destroy(Resource *resource)
{
    uninhibit(resource);
    wl_resource_destroy(resource->handle);
}

void ScreensaverInterfaceV1Private::inhibit(Resource *resource, const QString &application_name, const QString &reason_for_inhibit)
{
    wl_resource *res = resource->handle;
    if (inhibits.contains(res)) {
        wl_resource_post_error(res, TREELAND_SCREENSAVER_V1_ERROR_ALREADY_INHIBITED,
                               "Trying to inhibit with an existing inhibit active");
        return;
    }

    inhibits.insert(res, std::make_tuple(application_name, reason_for_inhibit));
    Helper::instance()->updateIdleInhibitor();
}

void ScreensaverInterfaceV1Private::uninhibit(Resource *resource)
{
    wl_resource *res = resource->handle;
    if (!inhibits.contains(res)) {
        wl_resource_post_error(res, TREELAND_SCREENSAVER_V1_ERROR_NOT_YET_INHIBITED,
                               "Trying to uninhibit but no active inhibit existed");
        return;
    }

    inhibits.remove(res);
    Helper::instance()->updateIdleInhibitor();
}

ScreensaverInterfaceV1::ScreensaverInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new ScreensaverInterfaceV1Private(this))
{
}

ScreensaverInterfaceV1::~ScreensaverInterfaceV1() = default;

QByteArrayView ScreensaverInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

bool ScreensaverInterfaceV1::isInhibited() const
{
    return !d->inhibits.isEmpty();
}

void ScreensaverInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void ScreensaverInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *ScreensaverInterfaceV1::global() const
{
    return d->global();
}

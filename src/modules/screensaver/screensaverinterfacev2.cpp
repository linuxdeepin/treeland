// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "screensaverinterfacev2.h"
#include "qwayland-server-treeland-screensaver-unstable-v2.h"
#include "core/waylandserver.h"

#include <wsocket.h>

#include <wayland-server.h>
#include <wayland-util.h>

#include <QDebug>
#include <QElapsedTimer>

WAYLIB_SERVER_USE_NAMESPACE

class ScreensaverInterfaceV2Private : public QtWaylandServer::treeland_screensaver_v2
{
public:
    explicit ScreensaverInterfaceV2Private(ScreensaverInterfaceV2 *_q);
    wl_global *global() const;

    struct InhibitEntry {
        QString applicationName;
        QString reason;
        QElapsedTimer age;
    };

    ScreensaverInterfaceV2 *q = nullptr;
    QHash<wl_resource*, InhibitEntry> inhibits;
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
    // destroy is a destructor request; release any active inhibit silently
    // (the protocol says disconnect auto-releases the inhibit). Do NOT call
    // uninhibit() here, which would post NOT_YET_INHIBITED when the client
    // already uninhibited before destroying the object.
    if (inhibits.remove(resource->handle))
        WaylandServer::instance()->updateIdleInhibitor();
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

    InhibitEntry entry;
    entry.applicationName = application_name;
    entry.reason = reason_for_inhibit;
    entry.age.start();
    inhibits.insert(res, entry);
    WaylandServer::instance()->updateIdleInhibitor();
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
    WaylandServer::instance()->updateIdleInhibitor();
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

QList<ScreensaverInterfaceV2::InhibitInfo> ScreensaverInterfaceV2::inhibits() const
{
    QList<InhibitInfo> result;
    result.reserve(d->inhibits.size());

    for (auto it = d->inhibits.constBegin(); it != d->inhibits.constEnd(); ++it) {
        InhibitInfo info;
        info.applicationName = it.value().applicationName;
        info.reason = it.value().reason;
        info.heldMs = static_cast<quint64>(it.value().age.isValid() ? it.value().age.elapsed() : 0);

        if (auto *client = wl_resource_get_client(it.key())) {
            if (auto *wclient = WClient::get(client))
                info.appId = QString::fromUtf8(wclient->appId());
            if (auto creds = WClient::getCredentials(client))
                info.pid = creds->pid;
        }
        result.append(info);
    }

    return result;
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

// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "prelaunchsplash.h"
#include "qwayland-server-treeland-prelaunch-splash-v1.h"

#include <wserver.h>

#include <qwdisplay.h>
#include <qwbuffer.h>

#include <QByteArray>
#include <QLoggingCategory>
#include <QMetaType>

#include <memory>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

Q_LOGGING_CATEGORY(prelaunchSplash, "treeland.prelaunchsplash", QtInfoMsg)

#define TREELAND_PRELAUNCH_SPLASH_MANAGER_V1_VERSION 1

class PrelaunchSplashPrivate : public QtWaylandServer::treeland_prelaunch_splash_manager_v1
{
public:
    explicit PrelaunchSplashPrivate(PrelaunchSplash *q)
        : q(q)
    {
    }

    wl_global *globalHandle() const
    {
        return this->m_global;
    }

protected:
    void treeland_prelaunch_splash_manager_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void treeland_prelaunch_splash_manager_v1_create_splash(
        Resource *resource,
        const QString &app_id,
        const QString &sandboxEngineName,
        struct ::wl_resource *icon_buffer) override
    {
        Q_UNUSED(resource);
        qCInfo(prelaunchSplash)
            << "create_splash request sandbox=" << sandboxEngineName << " app_id=" << app_id;
        auto qb = icon_buffer ? QW_NAMESPACE::qw_buffer::try_from_resource(icon_buffer) : nullptr;
        Q_EMIT q->splashRequested(app_id, qb);
    }

private:
    PrelaunchSplash *q;
};

PrelaunchSplash::PrelaunchSplash(QObject *parent)
    : QObject(parent)
    , d(new PrelaunchSplashPrivate(this))
{
}

PrelaunchSplash::~PrelaunchSplash() = default;

void PrelaunchSplash::create(WAYLIB_SERVER_NAMESPACE::WServer *server)
{
    if (d->isGlobal())
        return;
    // server->handle() returns qw_display* which can be implicitly converted to wl_display*
    d->init(*server->handle(), TREELAND_PRELAUNCH_SPLASH_MANAGER_V1_VERSION);
    qCDebug(prelaunchSplash) << "PrelaunchSplash global created";
}

void PrelaunchSplash::destroy(WAYLIB_SERVER_NAMESPACE::WServer *server)
{
    Q_UNUSED(server);
    if (!d->isGlobal())
        return;
    d->globalRemove();
    qCDebug(prelaunchSplash) << "PrelaunchSplash global removed";
}

wl_global *PrelaunchSplash::global() const
{
    return d->globalHandle();
}

QByteArrayView PrelaunchSplash::interfaceName() const
{
    return QtWaylandServer::treeland_prelaunch_splash_manager_v1::interfaceName();
}

// End of file

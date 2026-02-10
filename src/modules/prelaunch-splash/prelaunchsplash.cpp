// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "prelaunchsplash.h"
#include "qwayland-server-treeland-prelaunch-splash-v2.h"

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

#define TREELAND_PRELAUNCH_SPLASH_MANAGER_V2_VERSION 1

class SplashResource : public QtWaylandServer::treeland_prelaunch_splash_v2
{
public:
    SplashResource(PrelaunchSplash *owner,
                   struct ::wl_resource *resource,
                   const QString &appId,
                   const QString &instanceId)
        : QtWaylandServer::treeland_prelaunch_splash_v2(resource)
        , m_owner(owner)
        , m_appId(appId)
        , m_instanceId(instanceId)
    {
    }

    QString appId() const { return m_appId; }
    QString instanceId() const { return m_instanceId; }

protected:
    void treeland_prelaunch_splash_v2_destroy(Resource *resource) override
    {
        qCInfo(prelaunchSplash)
            << "Client destroy splash appId=" << m_appId << " instanceId=" << m_instanceId;
        wl_resource_destroy(resource->handle);
    }

    void treeland_prelaunch_splash_v2_destroy_resource(Resource *) override
    {
        // Covers both explicit destroy() and client crash/disconnect
        Q_EMIT m_owner->splashCloseRequested(m_appId, m_instanceId);
        delete this;
    }

private:
    PrelaunchSplash *m_owner;
    QString m_appId;
    QString m_instanceId;
};

class PrelaunchSplashPrivate : public QtWaylandServer::treeland_prelaunch_splash_manager_v2
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
    void treeland_prelaunch_splash_manager_v2_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void treeland_prelaunch_splash_manager_v2_create_splash(
        Resource *resource,
        uint32_t id,
        const QString &app_id,
        const QString &instance_id,
        const QString &sandboxEngineName,
        struct ::wl_resource *icon_buffer) override
    {
        qCInfo(prelaunchSplash)
            << "create_splash request sandbox=" << sandboxEngineName
            << " app_id=" << app_id << " instance_id=" << instance_id;

        auto *splashResource =
            wl_resource_create(resource->client(),
                               &treeland_prelaunch_splash_v2_interface,
                               wl_resource_get_version(resource->handle),
                               id);
        if (!splashResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }

        // SplashResource self-destructs via destroy_resource callback
        new SplashResource(q, splashResource, app_id, instance_id);

        auto qb = icon_buffer ? QW_NAMESPACE::qw_buffer::try_from_resource(icon_buffer) : nullptr;
        Q_EMIT q->splashRequested(app_id, instance_id, qb);
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
    d->init(*server->handle(), TREELAND_PRELAUNCH_SPLASH_MANAGER_V2_VERSION);
    qCDebug(prelaunchSplash) << "PrelaunchSplash v2 global created";
}

void PrelaunchSplash::destroy(WAYLIB_SERVER_NAMESPACE::WServer *server)
{
    Q_UNUSED(server);
    if (!d->isGlobal())
        return;
    d->globalRemove();
    qCDebug(prelaunchSplash) << "PrelaunchSplash v2 global removed";
}

wl_global *PrelaunchSplash::global() const
{
    return d->globalHandle();
}

QByteArrayView PrelaunchSplash::interfaceName() const
{
    return QtWaylandServer::treeland_prelaunch_splash_manager_v2::interfaceName();
}

// End of file

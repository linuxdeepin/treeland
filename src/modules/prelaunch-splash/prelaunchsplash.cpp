// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "prelaunchsplash.h"

#include "common/treelandlogging.h"
#include "qwayland-server-treeland-prelaunch-splash-unstable-v2.h"

#include <wlr_all.h>

#include <wserver.h>

#include <QByteArray>

WAYLIB_SERVER_USE_NAMESPACE

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

    QString appId() const
    {
        return m_appId;
    }

    QString instanceId() const
    {
        return m_instanceId;
    }

protected:
    void destroy(Resource *resource) override
    {
        qCInfo(lcTlPrelaunchSplash) << "Client destroy splash appId=" << m_appId
                                << " instanceId=" << m_instanceId;
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *) override
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
    explicit PrelaunchSplashPrivate(PrelaunchSplash *_q)
        : QtWaylandServer::treeland_prelaunch_splash_manager_v2()
        , q(_q)
    {
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

    void initSingletonGlobal(::wl_display *display, int version)
    {
        init(display, version);
        // The generated bind hook runs before bind() registers the resource in
        // resourceMap(), so posting the rejection there would leave a dangling
        // entry. Bind through our own global and reject before any Resource exists.
        wl_global_destroy(m_global);
        m_global = wl_global_create(display, interface(), version, this, bindSingleton);
    }

protected:
    void destroy_global() override
    {
        qCDebug(lcTlPrelaunchSplash) << "PrelaunchSplash v2 global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *resource) override
    {
        // Released by the bound client's last manager object or by its disconnect.
        if (resource->client() == m_boundClient && !resourceMap().contains(m_boundClient))
            m_boundClient = nullptr;
    }

    void create_splash(Resource *resource,
                       uint32_t id,
                       const QString &app_id,
                       const QString &instance_id,
                       const QString &sandboxEngineName,
                       struct ::wl_resource *icon_buffer) override
    {
        qCInfo(lcTlPrelaunchSplash) << "create_splash request sandbox=" << sandboxEngineName
                                << " app_id=" << app_id << " instance_id=" << instance_id;

        auto *splashResource = wl_resource_create(resource->client(),
                                                  &treeland_prelaunch_splash_v2_interface,
                                                  wl_resource_get_version(resource->handle),
                                                  id);
        if (!splashResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }

        // SplashResource self-destructs via destroy_resource callback
        new SplashResource(q, splashResource, app_id, instance_id);

        auto qb = icon_buffer ? wlr_buffer_try_from_resource(icon_buffer) : nullptr;
        Q_EMIT q->splashRequested(app_id, instance_id, qb);
    }

private:
    // The manager is a per-Session singleton: at most one client may bind it.
    // A second client is rejected instead of taking over the binding.
    static void bindSingleton(::wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<PrelaunchSplashPrivate *>(data);
        if (self->m_boundClient && self->m_boundClient != client) {
            auto *resource = wl_resource_create(client, interface(), version, id);
            if (resource) {
                wl_resource_post_error(resource,
                                       TREELAND_PRELAUNCH_SPLASH_MANAGER_V2_ERROR_BINDING_REJECTED,
                                       "treeland_prelaunch_splash_manager_v2 is already bound");
            }
            return;
        }
        self->m_boundClient = client;
        self->add(client, id, version);
    }

    PrelaunchSplash *q;
    struct ::wl_client *m_boundClient = nullptr;
};

PrelaunchSplash::PrelaunchSplash(QObject *parent)
    : QObject(parent)
    , d(new PrelaunchSplashPrivate(this))
{
}

PrelaunchSplash::~PrelaunchSplash() = default;

void PrelaunchSplash::create(WAYLIB_SERVER_NAMESPACE::WServer *server)
{
    d->initSingletonGlobal(server->handle(), InterfaceVersion);
    qCDebug(lcTlPrelaunchSplash) << "PrelaunchSplash v2 global created";
}

void PrelaunchSplash::destroy([[maybe_unused]] WAYLIB_SERVER_NAMESPACE::WServer *server)
{
    d->globalRemove();
    qCDebug(lcTlPrelaunchSplash) << "PrelaunchSplash v2 global removal scheduled";
}

wl_global *PrelaunchSplash::global() const
{
    return d->globalHandle();
}

QByteArrayView PrelaunchSplash::interfaceName() const
{
    return QtWaylandServer::treeland_prelaunch_splash_manager_v2::interfaceName();
}

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
        return m_splashGlobal;
    }

    wl_client *boundClientHandle() const
    {
        return m_boundClient;
    }

    // The generated bind is private and cannot reject, so the global is created
    // by hand: this interface is a per-session singleton (protocol requirement).
    void start(wl_display *display, uint32_t version)
    {
        m_splashGlobal = wl_global_create(display, interface(), version, this, bindCallback);
    }

    void stop()
    {
        if (!m_splashGlobal)
            return;
        // Immediate destruction is safe here, unlike the base class globalRemove()
        // which defers for 5 s to dodge the global-removal race (wayland#10):
        // both paths that reach this destroy the clients first — Session closes
        // its socket before detaching interfaces, WServer::stop() calls
        // wl_display_destroy_clients() — so no bind request can be in flight.
        wl_global_destroy(m_splashGlobal);
        m_splashGlobal = nullptr;
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *resource) override
    {
        // Covers both the destroy request and client crash/disconnect.
        if (resource->client() == m_boundClient)
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
    static void bindCallback(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        static_cast<PrelaunchSplashPrivate *>(data)->bindClient(client, id, version);
    }

    void bindClient(wl_client *client, uint32_t id, uint32_t version)
    {
        if (!m_boundClient) {
            m_boundClient = client;
            add(client, id, version);
            return;
        }

        qCDebug(lcTlPrelaunchSplash)
            << "Rejecting prelaunch splash manager bind: already bound by another client";
        // An error can only travel on a resource, so create the object for it
        // and destroy it again: a rejected bind leaves no state behind.
        auto *resource = wl_resource_create(client, interface(), version, id);
        if (!resource) {
            wl_client_post_no_memory(client);
            return;
        }
        wl_resource_post_error(resource,
                               TREELAND_PRELAUNCH_SPLASH_MANAGER_V2_ERROR_BINDING_REJECTED,
                               "%s",
                               "the prelaunch splash manager is already bound by another client");
        wl_resource_destroy(resource);
    }

    PrelaunchSplash *q;
    // Kept out of the generated base: its destruction path expects the display
    // listener that only the generated init() installs.
    wl_global *m_splashGlobal = nullptr;
    wl_client *m_boundClient = nullptr;
};

PrelaunchSplash::PrelaunchSplash(QObject *parent)
    : QObject(parent)
    , d(new PrelaunchSplashPrivate(this))
{
}

PrelaunchSplash::~PrelaunchSplash() = default;

void PrelaunchSplash::create(WAYLIB_SERVER_NAMESPACE::WServer *server)
{
    d->start(server->handle(), InterfaceVersion);
    qCDebug(lcTlPrelaunchSplash) << "PrelaunchSplash v2 global created";
}

void PrelaunchSplash::destroy([[maybe_unused]] WAYLIB_SERVER_NAMESPACE::WServer *server)
{
    d->stop();
    qCDebug(lcTlPrelaunchSplash) << "PrelaunchSplash v2 global destroyed";
}

wl_global *PrelaunchSplash::global() const
{
    return d->globalHandle();
}

wl_client *PrelaunchSplash::boundClient() const
{
    return d->boundClientHandle();
}

QByteArrayView PrelaunchSplash::interfaceName() const
{
    return QtWaylandServer::treeland_prelaunch_splash_manager_v2::interfaceName();
}

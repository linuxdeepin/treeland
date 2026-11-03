// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "prelaunchsplash.h"

#include "common/treelandlogging.h"
#include "qwayland-server-treeland-prelaunch-splash-unstable-v2.h"

#include <wlr/types/wlr_buffer.h>

#include <wserver.h>
#include <wsocket.h>

#include <QByteArray>
#include <QHash>

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

protected:
    void destroy_global() override
    {
        qCDebug(lcTlPrelaunchSplash) << "PrelaunchSplash v2 global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void bind_resource(Resource *resource) override
    {
        // The manager is a per-session singleton: treeland runs a single display
        // shared by all user sessions, so the binding must be scoped to the
        // client's root socket rather than the display.
        auto *socket = WSocket::get(resource->client());
        if (!socket)
            return;
        auto *root = socket->rootSocket();

        if (auto it = m_boundClientBySocket.constFind(root);
            it != m_boundClientBySocket.constEnd() && *it != resource->client()) {
            qCWarning(lcTlPrelaunchSplash)
                << "Rejecting prelaunch splash manager bind: already bound by another client";
            wl_resource_post_error(resource->handle,
                                   TREELAND_PRELAUNCH_SPLASH_MANAGER_V2_ERROR_BINDING_REJECTED,
                                   "treeland_prelaunch_splash_manager_v2 is already bound");
            return;
        }

        m_boundClientBySocket.insert(root, resource->client());
    }

    void destroy_resource(Resource *resource) override
    {
        // Released when the bound client destroys its last manager object or disconnects.
        // Only clear the binding if this client owns it: a rejected client also goes
        // through destroy_resource, and must not evict the legitimate owner.
        auto *socket = WSocket::get(resource->client());
        if (!socket)
            return;
        auto *root = socket->rootSocket();
        if (m_boundClientBySocket.value(root) == resource->client()
            && !resourceMap().contains(resource->client())) {
            m_boundClientBySocket.remove(root);
        }
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
    PrelaunchSplash *q;
    // The manager is a per-session singleton: at most one client per root socket
    // may bind it. A second client on the same session is rejected instead of
    // taking over the binding.
    QHash<WSocket *, struct ::wl_client *> m_boundClientBySocket;
};

PrelaunchSplash::PrelaunchSplash(QObject *parent)
    : QObject(parent)
    , d(new PrelaunchSplashPrivate(this))
{
}

PrelaunchSplash::~PrelaunchSplash() = default;

void PrelaunchSplash::create(WAYLIB_SERVER_NAMESPACE::WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
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

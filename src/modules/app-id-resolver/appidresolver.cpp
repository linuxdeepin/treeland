// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appidresolver.h"

#include "qwayland-server-treeland-app-id-resolver-v1.h"

#include <wserver.h>

#include <qwdisplay.h>

#include <QHash>
#include <QLoggingCategory>

#include <fcntl.h>
#include <unistd.h>
#include <utility>

Q_LOGGING_CATEGORY(treelandAppIdResolver, "treeland.appid.resolver", QtInfoMsg)

#define TREELAND_APP_ID_RESOLVER_MANAGER_V1_VERSION 1

WAYLIB_SERVER_USE_NAMESPACE

class AppIdResolver
    : public QObject
    , public QtWaylandServer::treeland_app_id_resolver_v1
{
public:
    AppIdResolver(QObject *parent, wl_client *client, int version, uint32_t id)
        : QObject(parent)
        , QtWaylandServer::treeland_app_id_resolver_v1(client, id, version)
    {
    }

    uint32_t requestResolve(int pidfd)
    {
        if (!m_alive || pidfd < 0)
            return 0;
        int dupfd = fcntl(pidfd, F_DUPFD_CLOEXEC, 0);
        if (dupfd < 0)
            return 0;
        uint32_t id = m_nextRequestId++;
        send_identify_request(id, dupfd);
        return id;
    }

    std::function<void(uint32_t, const QString &)> onResolved;
    std::function<void()> onDisconnected;

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource([[maybe_unused]] Resource *resource) override
    {
        m_alive = false;
        if (onDisconnected)
            onDisconnected();
    }

    void respond([[maybe_unused]] Resource *resource,
                 uint32_t request_id,
                 const QString &app_id,
                 [[maybe_unused]] const QString &sandboxEngineName) override
    {
        if (onResolved)
            onResolved(request_id, app_id);
    }

private:
    bool m_alive = true;
    uint32_t m_nextRequestId = 1;
};

class AppIdResolverManagerPrivate : public QtWaylandServer::treeland_app_id_resolver_manager_v1
{
public:
    explicit AppIdResolverManagerPrivate(AppIdResolverManager *_q)
        : QtWaylandServer::treeland_app_id_resolver_manager_v1()
        , q(_q)
    {
    }

    bool resolvePidfd(int pidfd, std::function<void(const QString &)> callback)
    {
        if (!m_resolver)
            return false;
        uint32_t id = m_resolver->requestResolve(pidfd);
        if (id == 0)
            return false;
        m_callbacks.insert(id, std::move(callback));
        return true;
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

private:
    AppIdResolverManager *q = nullptr;
    AppIdResolver *m_resolver = nullptr;
    QHash<uint32_t, std::function<void(const QString &)>> m_callbacks;

    void resolverGone()
    {
        if (!m_resolver)
            return;
        // All pending requests return empty appId when resolver disappears.
        auto callbacks = m_callbacks.values();
        m_callbacks.clear();
        for (auto &cb : std::as_const(callbacks)) {
            if (cb)
                cb(QString());
        }
        QObject::disconnect(m_resolver, nullptr, q, nullptr);
        m_resolver->deleteLater();
        m_resolver = nullptr;
        Q_EMIT q->availableChanged();
    }

    void handleResolved(uint32_t id, const QString &appId)
    {
        auto it = m_callbacks.find(id);
        if (it == m_callbacks.end())
            return;

        auto cb = it.value();
        m_callbacks.erase(it);
        if (cb)
            cb(appId);
    }

protected:
    void destroy_global() override
    {
        qCDebug(treelandAppIdResolver) << "AppIdResolverManager global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_resolver(Resource *resource, uint32_t id) override
    {
        qCDebug(treelandAppIdResolver) << "get_resolver called (client id):" << id;
        if (m_resolver) {
            wl_resource_post_error(resource->handle,
                                   WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "resolver already exists");
            return;
        }

        m_resolver = new AppIdResolver(q, resource->client(), resource->version(), id);
        m_resolver->onResolved = [this](uint32_t requestId, const QString &appId) {
            handleResolved(requestId, appId);
        };
        m_resolver->onDisconnected = [this]() {
            resolverGone();
        };
        Q_EMIT q->availableChanged();
        qCDebug(treelandAppIdResolver) << "Resolver bound";
    }
};

AppIdResolverManager::AppIdResolverManager(QObject *parent)
    : QObject(parent)
    , d(new AppIdResolverManagerPrivate(this))
{
}

AppIdResolverManager::~AppIdResolverManager() = default;

bool AppIdResolverManager::resolvePidfd(int pidfd, std::function<void(const QString &)> callback)
{
    return d->resolvePidfd(pidfd, std::move(callback));
}

void AppIdResolverManager::create(WServer *server)
{
    d->init(*server->handle(), TREELAND_APP_ID_RESOLVER_MANAGER_V1_VERSION);
    qCDebug(treelandAppIdResolver) << "AppIdResolverManager global created";
}

void AppIdResolverManager::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
    qCDebug(treelandAppIdResolver) << "AppIdResolverManager global removal scheduled";
}

wl_global *AppIdResolverManager::global() const
{
    return d->globalHandle();
}

QByteArrayView AppIdResolverManager::interfaceName() const
{
    return QtWaylandServer::treeland_app_id_resolver_manager_v1::interfaceName();
}

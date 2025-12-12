// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appidresolver.h"

#include <wserver.h>

#include <qwdisplay.h>

#include <QLoggingCategory>

#include <fcntl.h>
#include <unistd.h>
#include <utility>

Q_LOGGING_CATEGORY(treelandAppIdResolver, "treeland.appid.resolver", QtInfoMsg)

AppIdResolver::AppIdResolver(AppIdResolverManager *manager,
                             wl_client *client,
                             int version,
                             uint32_t id)
    : QObject(manager)
    , m_manager(manager)
{
    init(client, id, version);
}

AppIdResolver::~AppIdResolver() = default;

void AppIdResolver::treeland_app_id_resolver_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void AppIdResolver::treeland_app_id_resolver_v1_respond(Resource *resource,
                                                        uint32_t request_id,
                                                        const QString &app_id,
                                                        const QString &sandboxEngineName)
{
    Q_UNUSED(resource);
    Q_UNUSED(sandboxEngineName);
    Q_EMIT resolved(request_id, app_id);
}

uint32_t AppIdResolver::requestResolve(int pidfd)
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

// ---------------- Manager -----------------

AppIdResolverManager::AppIdResolverManager(QObject *parent)
    : QObject(parent)
{
}

void AppIdResolverManager::createGlobal(wl_display *display)
{
    if (m_display)
        return;
    m_display = display;
    init(m_display, 1);
    // QtWaylandServer base class sets up global internally; we cannot get wl_global* directly.
    qCDebug(treelandAppIdResolver) << "AppIdResolverManager global created";
}

void AppIdResolverManager::removeGlobal()
{
    if (!m_display)
        return;
    globalRemove();
    m_display = nullptr;
    qCDebug(treelandAppIdResolver) << "AppIdResolverManager global removed";
}

void AppIdResolverManager::treeland_app_id_resolver_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void AppIdResolverManager::treeland_app_id_resolver_manager_v1_get_resolver(Resource *resource,
                                                                            uint32_t id)
{
    qCDebug(treelandAppIdResolver) << "get_resolver called (client id):" << id;
    if (m_resolver) {
        wl_resource_post_error(resource->handle,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "resolver already exists");
        return;
    }
    m_resolver = new AppIdResolver(this, resource->client(), resource->version(), id);
    connect(m_resolver, &AppIdResolver::resolved, this, &AppIdResolverManager::handleResolved);
    connect(m_resolver, &AppIdResolver::disconnected, this, &AppIdResolverManager::resolverGone);
    Q_EMIT availableChanged();
    qCDebug(treelandAppIdResolver) << "Resolver bound";
}

void AppIdResolverManager::resolverGone()
{
    if (!m_resolver)
        return;
    // All pending requests (signalOnly and callback) return empty appId on resolver loss
    for (auto it = m_callbacks.begin(); it != m_callbacks.end(); ++it) {
        auto cb = it.value();
        if (cb)
            cb(QString());
        Q_EMIT appIdResolved(it.key(), QString());
    }
    m_callbacks.clear();
    for (auto id : std::as_const(m_signalOnlyPending)) {
        Q_EMIT appIdResolved(id, QString());
    }
    m_signalOnlyPending.clear();
    m_resolver->deleteLater();
    m_resolver = nullptr;
    Q_EMIT availableChanged();
}

void AppIdResolverManager::handleResolved(uint32_t id, const QString &appId)
{
    auto it = m_callbacks.find(id);
    if (it != m_callbacks.end()) {
        auto cb = it.value();
        m_callbacks.erase(it);
        if (cb)
            cb(appId);
        Q_EMIT appIdResolved(id, appId);
        return;
    }
    if (m_signalOnlyPending.contains(id)) {
        m_signalOnlyPending.remove(id);
        Q_EMIT appIdResolved(id, appId);
    }
}

bool AppIdResolverManager::resolvePidfd(int pidfd, std::function<void(const QString &)> callback)
{
    if (!m_resolver)
        return false;
    uint32_t id = m_resolver->requestResolve(pidfd);
    if (id == 0)
        return false;
    m_callbacks.insert(id, std::move(callback));
    return true;
}

// ---------------- WServerInterface -----------------

void AppIdResolverManager::create(WServer *server)
{
    if (!server || m_display)
        return;
    createGlobal(*server->handle());
}

void AppIdResolverManager::destroy(WServer *server)
{
    Q_UNUSED(server);
    removeGlobal();
}

wl_global *AppIdResolverManager::global() const
{
    // QtWayland private base does not expose the wl_global. Returning nullptr is acceptable;
    // other modules in tree also sometimes return handle->global. We can't here.
    return nullptr;
}

QByteArrayView AppIdResolverManager::interfaceName() const
{
    return "treeland_app_id_resolver_manager_v1";
}

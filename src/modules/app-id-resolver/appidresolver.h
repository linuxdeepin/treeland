// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-server-treeland-app-id-resolver-v1.h"

#include <wayland-server-core.h>
#include <wglobal.h>
#include <wserver.h>

#include <QHash>
#include <QObject>
#include <QPointer>

#include <functional>

WAYLIB_SERVER_USE_NAMESPACE

class AppIdResolverManager;

class AppIdResolver
    : public QObject
    , public QtWaylandServer::treeland_app_id_resolver_v1
{
    Q_OBJECT
public:
    AppIdResolver(AppIdResolverManager *manager, wl_client *client, int version, uint32_t id);
    ~AppIdResolver() override;

    uint32_t requestResolve(int pidfd); // returns request id or 0

Q_SIGNALS:
    void resolved(uint32_t requestId, const QString &appId);
    void disconnected();

protected:
    void treeland_app_id_resolver_v1_destroy(Resource *resource) override;
    void treeland_app_id_resolver_v1_respond(Resource *resource,
                                             uint32_t request_id,
                                             const QString &app_id,
                                             const QString &sandboxEngineName) override;

private:
    AppIdResolverManager *m_manager;
    bool m_alive = true;
    uint32_t m_nextRequestId = 1;
};

class AppIdResolverManager
    : public QObject
    , public QtWaylandServer::treeland_app_id_resolver_manager_v1
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit AppIdResolverManager(QObject *parent = nullptr);
    ~AppIdResolverManager() override = default;

    // WServer attach-style lifecycle
    void createGlobal(wl_display *display); // kept for internal use
    void removeGlobal();

    // Callback-based API: returns true if request started, false if no resolver or dup fd failed
    // Callback is invoked asynchronously on the Wayland (main) thread; empty string if resolver disconnects
    bool resolvePidfd(int pidfd, std::function<void(const QString &)> callback);

Q_SIGNALS:
    void availableChanged();
    void appIdResolved(uint32_t requestId, const QString &appId); // 解析完成

protected: // protocol generated virtuals
    void treeland_app_id_resolver_manager_v1_destroy(Resource *resource) override;
    void treeland_app_id_resolver_manager_v1_get_resolver(Resource *resource, uint32_t id) override;

protected: // WServerInterface overrides
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override; // might be nullptr (QtWayland hides it)
    QByteArrayView interfaceName() const override;

private:
    wl_display *m_display = nullptr;
    AppIdResolver *m_resolver = nullptr;
    // request id -> callback
    QHash<uint32_t, std::function<void(const QString &)>> m_callbacks;
    // For legacy signal interface only (ids from resolvePidfd(int) stored here)
    QSet<uint32_t> m_signalOnlyPending;
    wl_global *m_global = nullptr; // best-effort cache (not exposed by QtWayland API directly)

    void resolverGone();
    void handleResolved(uint32_t id, const QString &appId);
};

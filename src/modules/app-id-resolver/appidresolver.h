// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <functional>

#include <wserver.h>

// Waylib server namespace macros & basic types
#include <wglobal.h>
// wl_client forward declaration and core protocol types
#include <wayland-server-core.h>

#include "qwayland-server-treeland-app-id-resolver-v1.h"

WAYLIB_SERVER_USE_NAMESPACE

class AppIdResolverManager;

class AppIdResolver : public QObject, public QtWaylandServer::treeland_app_id_resolver_v1
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

class AppIdResolverManager : public QObject,
                             public QtWaylandServer::treeland_app_id_resolver_manager_v1,
                             public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit AppIdResolverManager(QObject *parent = nullptr);
    ~AppIdResolverManager() override = default;

    // WServer attach-style lifecycle
    void createGlobal(wl_display *display); // kept for internal use
    void removeGlobal();

    // 回调式接口：成功发起返回 true，失败(无 resolver 或 dup fd 失败) 返回 false
    // 回调在 Wayland 线程(同主线程)异步触发；若 resolver 断开则回调收到空字符串。
    bool resolvePidfd(int pidfd, std::function<void(const QString &)> callback);

    // 兼容信号式接口（如果仍需监听所有结果，可连接这个信号）。但 ShellHandler 现在改回只使用回调。
    uint32_t resolvePidfd(int pidfd); // Deprecated: 保留以免其他模块已使用

Q_SIGNALS:
    void availableChanged();
    void appIdResolved(uint32_t requestId, const QString &appId); // 解析完成

protected: // protocol generated virtuals
    void treeland_app_id_resolver_manager_v1_destroy(Resource *resource) override;
    void treeland_app_id_resolver_manager_v1_get_resolver(Resource *resource,
                                                          uint32_t id) override;

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
    // 仅用于兼容旧信号接口的监听（通过 resolvePidfd(int) 返回 id 存入此集合）
    QSet<uint32_t> m_signalOnlyPending;
    wl_global *m_global = nullptr; // best-effort cache (not exposed by QtWayland API directly)

    void resolverGone();
    void handleResolved(uint32_t id, const QString &appId);
};

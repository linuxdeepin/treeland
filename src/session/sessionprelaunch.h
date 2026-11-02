// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "sessioncomponent.h"
#include "surface/surfacewrapper.h"
#include <QHash>
#include <QSet>
#include <memory>
#include <functional>

class PrelaunchSplash;
class AppIdResolverManager;
class WaylandServer;
class WindowConfigStore;
struct wlr_buffer;

// A fixed launch domain: requests, cancellation and matching never share appId
// state with another user's desktop, including asynchronous config callbacks.
class SessionPrelaunch : public SessionComponent
{
    Q_OBJECT
public:
    SessionPrelaunch(Session *owner, WaylandServer *runtime);
    bool needsResolution() const;
    bool consumeClosed(const QString &appId);
    SurfaceWrapper *takeSplash(const QString &appId,
                              WAYLIB_SERVER_NAMESPACE::WToplevelSurface *surface,
                              SurfaceWrapper::Type type);
    PrelaunchSplash *protocol() const;
    bool resolvePidfd(int pidfd, std::function<void(const QString &)> callback);
Q_SIGNALS:
    void splashCreated(SurfaceWrapper *wrapper);
private:
    using Icon = std::shared_ptr<wlr_buffer>;
    void request(const QString &appId, const QString &instanceId, wlr_buffer *buffer);
    void close(const QString &appId, const QString &instanceId);
    void create(const QString &appId, const QString &instanceId, quint64 generation,
                const Icon &icon, const QSize &size, const QString &darkPalette,
                const QString &lightPalette, qlonglong theme);
    void discard(SurfaceWrapper *wrapper, bool closeWindow);
    struct Pending { QString instanceId; quint64 generation; };
    struct Splash { QPointer<SurfaceWrapper> wrapper; QString instanceId; };
    WaylandServer *const m_runtime;
    WindowConfigStore *const m_store;
    QPointer<PrelaunchSplash> m_protocol;
    QPointer<AppIdResolverManager> m_resolver;
    QHash<QString, Pending> m_pending;
    QList<Splash> m_splashes;
    QSet<QString> m_closed;
    quint64 m_generation = 0;
};

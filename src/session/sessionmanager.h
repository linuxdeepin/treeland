// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "session.h"

#include <memory>

/**
 * SessionManager manages user sessions, including their associated
 * WSocket and WXWayland instances.
 */
class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager();

    const QList<std::shared_ptr<Session>> &sessions() const;
    std::weak_ptr<Session> activeSession() const;
    std::shared_ptr<Session> sessionForClient(WClient *client) const;
    std::shared_ptr<Session> globalSession() const;

    bool activeSocketEnabled() const;
    void setActiveSocketEnabled(bool newEnabled);

    struct ActiveSessionUpdate
    {
        std::shared_ptr<Session> session;
        bool activeSessionChanged = false;

        explicit operator bool() const noexcept
        {
            return session != nullptr;
        }
    };

    ActiveSessionUpdate prepareActiveUserSession(const QString &username, const QString &id);
    void commitActiveUserSession(const ActiveSessionUpdate &update);
    void updateActiveUserSession(const QString &username, const QString &id);
    void removeSession(std::shared_ptr<Session> session);
    std::shared_ptr<Session> sessionForId(const QString &id) const;
    std::shared_ptr<Session> sessionForUid(uid_t uid) const;
    std::shared_ptr<Session> sessionForUser(const QString &username) const;
    std::shared_ptr<Session> sessionForXWayland(WXWayland *xwayland) const;
    std::shared_ptr<Session> sessionForSocket(WSocket *socket) const;
    bool isDDEUserClient(WClient *client);
    void syncActiveSessionCursorSettings();
    void syncActiveSessionXWaylandPrimaryOutput();

Q_SIGNALS:
    void socketFileChanged();
    void sessionChanged();
    void xwaylandAuthChanged();

private:
    std::shared_ptr<Session> ensureSession(const QString &id, QString username);

    std::weak_ptr<Session> m_activeSession;
    QList<std::shared_ptr<Session>> m_sessions;
    WallpaperLauncher *m_wallpaperLauncher { nullptr };
};

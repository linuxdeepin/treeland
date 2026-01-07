// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <xcb/xproto.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSocket;
class WXWayland;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class SettingManager;

class Session : public QObject {
    Q_OBJECT
public:
    ~Session();

    int id() const;
    uid_t uid() const;
    const QString &username() const;
    WSocket *socket() const;
    WXWayland *xwayland() const;
    quint32 noTitlebarAtom() const;

Q_SIGNALS:
    void aboutToBeDestroyed();

private:
    friend class SessionManager;

    int m_id = 0;
    uid_t m_uid = 0;
    QString m_username = {};
    WSocket *m_socket = nullptr;
    WXWayland *m_xwayland = nullptr;
    quint32 m_noTitlebarAtom = XCB_ATOM_NONE;
    SettingManager *m_settingManager = nullptr;
    QThread *m_settingManagerThread = nullptr;
};

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
    std::shared_ptr<Session> globalSession() const;

    bool activeSocketEnabled() const;
    void setActiveSocketEnabled(bool newEnabled);

    void updateActiveUserSession(const QString &username, int id);
    void removeSession(std::shared_ptr<Session> session);
    std::shared_ptr<Session> sessionForId(int id) const;
    std::shared_ptr<Session> sessionForUid(uid_t uid) const;
    std::shared_ptr<Session> sessionForUser(const QString &username) const;
    std::shared_ptr<Session> sessionForXWayland(WXWayland *xwayland) const;
    std::shared_ptr<Session> sessionForSocket(WSocket *socket) const;

Q_SIGNALS:
    void socketFileChanged();
    void sessionChanged();

private:
    std::shared_ptr<Session> ensureSession(int id, QString username);

    std::weak_ptr<Session> m_activeSession;
    QList<std::shared_ptr<Session>> m_sessions;
};

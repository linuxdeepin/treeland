// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <xcb/xproto.h>
#include <QtQml/qqmlregistration.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServerInterface;
class WClient;
class WSocket;
class WXWayland;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class SettingManager;
class WallpaperLauncher;

class Session : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged FINAL)
public:
    ~Session();

    const QString &id() const;
    uid_t uid() const;
    const QString &username() const;
    WSocket *socket() const;
    WXWayland *xwayland() const;
    quint32 noTitlebarAtom() const;
    bool isActive() const;
    bool ownsClient(WClient *client) const;
    void attachInterface(WServerInterface *interface);
    void detachInterface(WServerInterface *interface);

Q_SIGNALS:
    void activeChanged(bool active);
    void aboutToBeDestroyed();

private:
    friend class SessionManager;
    void setActive(bool active);
    bool m_active = false;

    // The logind session id. Set once in ensureSession() and never mutated
    // afterwards; it is read from QThreadPool workers (onSessionLock/Unlock),
    // so it must not change after creation.
    QString m_id;
    uid_t m_uid = 0;
    QString m_username = {};
    QList<WServerInterface *> m_interfaces;
    WSocket *m_socket = nullptr;
    WXWayland *m_xwayland = nullptr;
    quint32 m_noTitlebarAtom = XCB_ATOM_NONE;
    SettingManager *m_settingManager = nullptr;
    QThread *m_settingManagerThread = nullptr;
};

#include "sessionmanager.h"

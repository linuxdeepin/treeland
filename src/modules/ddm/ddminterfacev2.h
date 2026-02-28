// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wserver.h"

#include <Session.h>

/**
 * Server-side wrapper for treeland-ddm protocol.
 */
class DDMInterfaceV2 : public QObject, public Waylib::Server::WServerInterface
{
    Q_OBJECT
public:
    QByteArrayView interfaceName() const override;
    void setHandle(struct wl_resource *handle);
    static QString authErrorToString(uint32_t error);

    ////////////////////
    // Event Wrappers //
    ////////////////////

public Q_SLOTS:
    void login(const QString &username,
               const QString &password,
               DDM::Session::Type sessionType,
               const QString &sessionFile) const;
    void logout(const QString &session) const;
    void lock(const QString &session) const;
    void unlock(const QString &session, const QString &password) const;
    void powerOff() const;
    void reboot() const;
    void suspend() const;
    void hibernate() const;
    void hybridSleep() const;
    void switchToVt(int vtnr) const;

Q_SIGNALS:
    void connected();
    void disconnected();

    ///////////////////////////
    // Signals from Requests //
    ///////////////////////////

    void capabilities(uint32_t capabilities);
    void userLoggedIn(const QString &username, const QString &session);
    void authenticationFailed(uint32_t error);

protected:
    void create(Waylib::Server::WServer *server) override;
    void destroy(Waylib::Server::WServer *server) override;
    wl_global *global() const override;

private:
    struct wl_global *m_global{ nullptr };
};

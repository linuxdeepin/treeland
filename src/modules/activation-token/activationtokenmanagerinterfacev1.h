// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <wayland-server-core.h>

#include <QObject>

#include <memory>

class ActivationManagerInterfaceV1;
class ActivationTokenManagerInterfaceV1Private;

/**
 * Source of xdg-activation-v1 tokens for dde-application-manager.
 *
 * The compositor neither verifies the caller's identity nor enforces the
 * singleton binding yet: one instance belongs to one Session, next to that
 * Session's ActivationManagerInterfaceV1, so both the global and the token
 * table are scoped to one user's socket. Any client of the Session may bind
 * and mint tokens; deciding which application may be activated stays with the
 * caller.
 *
 * TODO: verify the caller by app id instead of trusting any client of the
 * Session, and enforce the singleton binding required by the protocol XML.
 * Once appid-based authentication exists, reject binds whose app id is not the
 * DDE application manager.
 */
class ActivationTokenManagerInterfaceV1 : public QObject,
                                          public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit ActivationTokenManagerInterfaceV1(ActivationManagerInterfaceV1 *activation,
                                               QObject *parent = nullptr);
    ~ActivationTokenManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<ActivationTokenManagerInterfaceV1Private> d;
};

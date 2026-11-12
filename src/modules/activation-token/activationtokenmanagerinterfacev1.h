// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <wayland-server-core.h>

#include <QObject>

#include <memory>

class ActivationManagerInterfaceV1;
class ActivationTokenManagerInterfaceV1Private;
class Session;

/**
 * Privileged source of xdg-activation-v1 tokens for dde-application-manager.
 *
 * The compositor only verifies that the calling client is the application
 * manager, i.e. the client holding this Session's
 * treeland-prelaunch-splash-unstable-v2 manager; deciding which application may
 * be activated stays with it. One instance belongs to one Session, next to that
 * Session's ActivationManagerInterfaceV1, so both the global and the token
 * table are scoped to one user: another user's application manager can bind its
 * own instance at the same time and never sees these tokens.
 *
 * The check runs at bind time: a client that does not hold this Session's
 * splash manager is rejected with not_privileged, and a second client while the
 * manager is bound is rejected with binding_rejected. Clients must therefore
 * bind the splash manager first.
 */
class ActivationTokenManagerInterfaceV1 : public QObject,
                                          public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit ActivationTokenManagerInterfaceV1(ActivationManagerInterfaceV1 *activation,
                                               Session *session,
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

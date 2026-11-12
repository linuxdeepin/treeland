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
 * The compositor does not verify the caller's identity yet: one instance
 * belongs to one Session, next to that Session's ActivationManagerInterfaceV1,
 * so both the global and the token table are scoped to one user, and within a
 * Session the manager is a singleton (a second concurrent bind is rejected with
 * binding_rejected). Deciding which application may be activated stays with the
 * calling client.
 *
 * TODO: verify the caller by app id instead of trusting any client of the
 * Session. The current version accepts any client of the Session on a
 * first-come basis; once appid-based authentication exists, reject binds whose
 * app id is not the DDE application manager.
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

// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

class ActivationManagerInterfaceV1Private;

/**
 * @brief Treeland private xdg-activation implementation.
 *
 * Provides the treeland_activation_manager_v1 Wayland interface.  Clients use
 * it to request compositor-mediated window activation via a token handshake:
 *
 *   1. Requesting client calls get_token() → populates token attributes →
 *      calls commit() → receives a unique token string via the done event.
 *   2. Token string is forwarded to the target application.
 *   3. Target application calls activate(token, surface) → compositor emits
 *      activateRequested(token, surface).
 *
 * Version 1 uses permissive token validation: any committed token is accepted
 * regardless of serial or focus state.
 */
class ActivationManagerInterfaceV1 : public QObject, public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit ActivationManagerInterfaceV1(QObject *parent = nullptr);
    ~ActivationManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    /**
     * Emitted when a client requests activation of @p surface using @p token.
     * The compositor should activate the surface when it considers the request
     * valid (v1: all committed tokens are considered valid).
     */
    void activateRequested(const QString &token,
                           WAYLIB_SERVER_NAMESPACE::WSurface *surface);

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<ActivationManagerInterfaceV1Private> d;
};

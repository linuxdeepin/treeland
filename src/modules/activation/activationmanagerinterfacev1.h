// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QRectF>
#include <QObject>

#include <functional>
#include <memory>
#include <optional>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
class WSeat;
WAYLIB_SERVER_END_NAMESPACE

class ActivationManagerInterfaceV1Private;

class ActivationManagerInterfaceV1 : public QObject, public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    enum class TokenDisposition
    {
        Invalid,
        Attention,
        Active,
    };
    Q_ENUM(TokenDisposition)

    using LaunchRectProvider = std::function<std::optional<QRectF>(wl_resource *tokenResource)>;

    explicit ActivationManagerInterfaceV1(
        std::function<bool(WAYLIB_SERVER_NAMESPACE::WSurface *, WAYLIB_SERVER_NAMESPACE::WSeat *)> trustedSurfaceChecker,
        QObject *parent = nullptr);
    ~ActivationManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

    /**
     * Sets a provider that returns a committed launch rectangle for a given
     * xdg_activation_token_v1 resource. Called during token commit so the
     * rectangle can be stored with the token and later used when activate()
     * is called.
     */
    void setLaunchRectProvider(LaunchRectProvider provider);

Q_SIGNALS:
    // The server emits one signal with precomputed disposition for policy handling.
    // seat is the WSeat associated with the token via set_serial, or null.
    // originatingSurface is the WSurface that was set via set_surface on the
    // token (the surface that initiated the launch), or null.
    // launchRect is the animation source rectangle in coordinates relative to
    // originatingSurface, or an invalid QRectF when no window animation is
    // requested for this token.
    void activateRequested(TokenDisposition disposition,
                           WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                           WAYLIB_SERVER_NAMESPACE::WSeat *seat,
                           WAYLIB_SERVER_NAMESPACE::WSurface *originatingSurface,
                           QRectF launchRect);

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<ActivationManagerInterfaceV1Private> d;
};

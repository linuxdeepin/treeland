// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>

#include <functional>
#include <memory>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
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

    explicit ActivationManagerInterfaceV1(
        std::function<bool(WAYLIB_SERVER_NAMESPACE::WSurface *)> trustedSurfaceChecker,
        QObject *parent = nullptr);
    ~ActivationManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    // The server emits one signal with precomputed disposition for policy handling.
    void activateRequested(TokenDisposition disposition,
                           WAYLIB_SERVER_NAMESPACE::WSurface *surface);

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<ActivationManagerInterfaceV1Private> d;
};

// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>
#include <QRectF>
#include <memory>
#include <optional>

class LaunchAnimationManagerInterfaceV1Private;

class LaunchAnimationManagerInterfaceV1
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit LaunchAnimationManagerInterfaceV1(QObject *parent = nullptr);
    ~LaunchAnimationManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

    /**
     * Called by the activation manager when a token is about to be committed.
     * If a launch rect has been committed for this xdg_activation_token_v1
     * resource, returns and removes it. Returns std::nullopt otherwise.
     */
    std::optional<QRectF> takeCommittedRect(wl_resource *tokenResource);

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    friend class LaunchRectV1;
    std::unique_ptr<LaunchAnimationManagerInterfaceV1Private> d;
};

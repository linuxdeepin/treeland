// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>
#include <QQmlEngine>

class TreelandUserConfig;

WAYLIB_SERVER_USE_NAMESPACE

/**
 * @brief Server implementation of treeland_appearance_unstable_v1.
 *
 * Read-only global that pushes user-level appearance settings to all
 * bound clients. On bind the current value of every setting is sent
 * immediately; afterwards events are broadcast whenever a setting
 * changes (either via the privileged AppearanceManagerInterfaceV1 or
 * via DConfig).
 */
class AppearanceInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit AppearanceInterfaceV1(QObject *parent = nullptr);
    ~AppearanceInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    /// Push the current value of every setting to @p resource.
    void sendAll(wl_resource *resource);

    static constexpr int InterfaceVersion = 1;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    void setupConfigConnections();

    struct Private;
    std::unique_ptr<Private> d;
};

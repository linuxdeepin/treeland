// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>
#include <QQmlEngine>

class TreelandUserConfig;

WAYLIB_SERVER_USE_NAMESPACE

/**
 * @brief Server implementation of treeland_appearance_manager_unstable_v1.
 *
 * Privileged global that allows the control center / system settings
 * to modify user-level appearance settings. Each set_* request applies
 * the change immediately to DConfig and broadcasts the new value to all
 * clients bound to AppearanceInterfaceV1.
 */
class AppearanceManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit AppearanceManagerInterfaceV1(QObject *parent = nullptr);
    ~AppearanceManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

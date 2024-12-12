// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "interfaces/lockscreeninterface.h"
#include "interfaces/plugininterface.h"

#include <QPointer>
#include <QQuickItem>

class LockScreenPlugin
    : public QObject
    , public PluginInterface
    , public ILockScreen
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.deepin.treeland.plugin.lockscreen/1.0" FILE "metadata.json")
    Q_INTERFACES(PluginInterface ILockScreen)
public:
    QString name() const override
    {
        return QStringLiteral("LockScreen");
    }

    QString description() const override
    {
        return QStringLiteral("LockScreen plugin");
    }

    void initialize(TreelandProxyInterface *proxy) override;
    void shutdown() override;

    bool enabled() const override
    {
        return true;
    }

    QQuickItem *createLockScreen(Output *output, QQuickItem *parent) override;

private:
    TreelandProxyInterface *m_proxy;
    QQmlComponent m_lockscreenComponent;
};

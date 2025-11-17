// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "multitaskview.h"
#include "interfaces/multitaskviewinterface.h"
#include "interfaces/plugininterface.h"

#include <QPointer>
#include <QQuickItem>

class MultitaskViewPlugin
    : public QObject
    , public PluginInterface
    , public IMultitaskView
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.deepin.treeland.plugin.multitaskview/1.0" FILE "metadata.json")
    Q_INTERFACES(PluginInterface IMultitaskView)
public:
    QString name() const override
    {
        return QStringLiteral("MultitaskView");
    }

    QString description() const override
    {
        return QStringLiteral("MultitaskView plugin");
    }

    void initialize(TreelandProxyInterface *proxy) override;
    void shutdown() override;

    bool enabled() const override
    {
        return true;
    }

    void setStatus(IMultitaskView::Status status) override;
    void updatePartialFactor(qreal delta) override;
    void toggleMultitaskView(IMultitaskView::ActiveReason reason) override;
    void immediatelyExit() override;

private:
    QQuickItem *createMultitaskview(QQuickItem *parent);

private:
    TreelandProxyInterface *m_proxy;
    QQmlComponent m_multitaskViewComponent;
    QPointer<Multitaskview> m_multitaskview{ nullptr };
};

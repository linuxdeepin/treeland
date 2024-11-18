#pragma once

#include "multitaskview.h"
#include "multitaskviewinterface.h"
#include "plugininterface.h"

#include <QPointer>
#include <QQuickItem>

class MultitaskViewPlugin
    : public QObject
    , public PluginInterface
    , public IMultitaskView
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.deepin.treeland.plugin.multitaskview/1.0")
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

    void toggleMultitaskView(IMultitaskView::ActiveReason reason) override;

private:
    QQuickItem *createMultitaskview(QQuickItem *parent);

private:
    TreelandProxyInterface *m_proxy;
    QQmlComponent m_multitaskViewComponent;
    QPointer<Multitaskview> m_multitaskview{ nullptr };
};

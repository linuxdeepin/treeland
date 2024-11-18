#include "multitaskviewplugin.h"

#include "helper.h"
#include "multitaskview.h"
#include "proxyinterface.h"
#include "qmlengine.h"
#include "rootsurfacecontainer.h"
#include "workspace.h"

void MultitaskViewPlugin::initialize(TreelandProxyInterface *proxy)
{
    m_proxy = proxy;

    new (&m_multitaskViewComponent)
        QQmlComponent(m_proxy->qmlEngine(), "MultitaskView", "MultitaskviewProxy", this);
}

void MultitaskViewPlugin::shutdown()
{
    m_proxy = nullptr;
}

QQuickItem *MultitaskViewPlugin::createMultitaskview(QQuickItem *parent)
{
    return m_proxy->qmlEngine()->createComponent(m_multitaskViewComponent, parent);
}

void MultitaskViewPlugin::toggleMultitaskView(IMultitaskView::ActiveReason reason)
{
    if (!m_multitaskview) {
        Helper::instance()->toggleOutputMenuBar(false);
        m_proxy->workspace()->setSwitcherEnabled(false);
        m_multitaskview =
            qobject_cast<Multitaskview *>(createMultitaskview(m_proxy->rootSurfaceContainer()));
        connect(m_multitaskview.data(), &Multitaskview::visibleChanged, this, [this] {
            if (!m_multitaskview->isVisible()) {
                m_multitaskview->deleteLater();
                Helper::instance()->toggleOutputMenuBar(true);
                m_proxy->workspace()->setSwitcherEnabled(true);
            }
        });
        Helper::instance()->setCurrentMode(Helper::CurrentMode::Multitaskview);
        m_multitaskview->enter(static_cast<Multitaskview::ActiveReason>(reason));
    } else {
        if (m_multitaskview->status() == Multitaskview::Exited) {
            m_multitaskview->enter(Multitaskview::ShortcutKey);
            Helper::instance()->setCurrentMode(Helper::CurrentMode::Multitaskview);
        } else {
            m_multitaskview->exit(nullptr);
            Helper::instance()->setCurrentMode(Helper::CurrentMode::Normal);
        }
    }
}

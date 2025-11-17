// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "multitaskviewplugin.h"

#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "interfaces/proxyinterface.h"
#include "multitaskview.h"
#include "seat/helper.h"
#include "workspace/workspace.h"

void MultitaskViewPlugin::initialize(TreelandProxyInterface *proxy)
{
    m_proxy = proxy;

    new (&m_multitaskViewComponent)
        QQmlComponent(m_proxy->qmlEngine(), "MultitaskView", "MultitaskviewProxy", this);
}

void MultitaskViewPlugin::shutdown()
{
    m_proxy = nullptr;

    if (m_multitaskview) {
        m_multitaskview->exit();
    }
}

QQuickItem *MultitaskViewPlugin::createMultitaskview(QQuickItem *parent)
{
    return m_proxy->qmlEngine()->createComponent(m_multitaskViewComponent, parent);
}

void MultitaskViewPlugin::setStatus(IMultitaskView::Status status)
{
    if (m_multitaskview)
        m_multitaskview->setStatus(static_cast<Multitaskview::Status>(status));
}

void MultitaskViewPlugin::toggleMultitaskView(IMultitaskView::ActiveReason reason)
{
    if (!m_multitaskview) {
        m_proxy->workspace()->setSwitcherEnabled(false);
        m_multitaskview =
            qobject_cast<Multitaskview *>(createMultitaskview(m_proxy->rootSurfaceContainer()));
        connect(m_multitaskview.data(), &Multitaskview::visibleChanged, this, [this] {
            if (!m_multitaskview->isVisible()) {
                m_multitaskview->deleteLater();
                m_proxy->workspace()->setSwitcherEnabled(true);
            }
        });

        m_multitaskview->enter(static_cast<Multitaskview::ActiveReason>(reason));
    } else {
        if (reason == IMultitaskView::Gesture) {
            if (m_multitaskview->status() == Multitaskview::Exited) {
                m_multitaskview->exit(nullptr);
            } else {
                m_multitaskview->enter(static_cast<Multitaskview::ActiveReason>(reason));
            }
        } else {
            if (m_multitaskview->status() == Multitaskview::Exited) {
                m_multitaskview->enter(static_cast<Multitaskview::ActiveReason>(reason));
            } else {
                m_multitaskview->exit(nullptr);
            }
        }
    }
}

void MultitaskViewPlugin::updatePartialFactor(qreal delta)
{
    if (m_multitaskview) {
        m_multitaskview->updatePartialFactor(delta);
    }
}

void MultitaskViewPlugin::immediatelyExit()
{
    if (m_multitaskview) {
        m_multitaskview->exit(nullptr, true);
    }
}

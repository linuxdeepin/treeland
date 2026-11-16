// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "multitaskviewplugin.h"

#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "interfaces/proxyinterface.h"
#include "multitaskview.h"
#include "core/waylandserver.h"
#include "desktop/desktopenvironment.h"

void MultitaskViewPlugin::initialize(TreelandProxyInterface *proxy)
{
    m_proxy = proxy;

    new (&m_multitaskViewComponent)
        QQmlComponent(m_proxy->qmlEngine(), "MultitaskView", "MultitaskviewProxy", this);
}

void MultitaskViewPlugin::shutdown()
{
    if (m_proxy)
        WaylandServer::instance()->setMultitaskViewFactory(nullptr);
    m_proxy = nullptr;
}

QQuickItem *MultitaskViewPlugin::createMultitaskView(DesktopEnvironment *desktop)
{
    return m_proxy->qmlEngine()->createComponent(
        m_multitaskViewComponent, desktop->viewRoot(),
        {{"desktop", QVariant::fromValue(desktop)}});
}

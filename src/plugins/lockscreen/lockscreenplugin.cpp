// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "lockscreenplugin.h"

#include "helper.h"
#include "output.h"
#include "proxyinterface.h"
#include "qmlengine.h"

void LockScreenPlugin::initialize(TreelandProxyInterface *proxy)
{
    m_proxy = proxy;

    new (&m_lockscreenComponent) QQmlComponent(m_proxy->qmlEngine(), "LockScreen", "Greeter", this);
}

void LockScreenPlugin::shutdown()
{
    m_proxy = nullptr;
}

QQuickItem *LockScreenPlugin::createLockScreen(Output *output, QQuickItem *parent)
{
    return m_proxy->qmlEngine()->createComponent(
        m_lockscreenComponent,
        parent,
        { { "output", QVariant::fromValue(output->output()) },
          { "outputItem", QVariant::fromValue(output->outputItem()) } });
}

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessionswitcher.h"
#include "modules/shortcut/shortcutrunner.h"
#include "desktop/desktopenvironment.h"
#include "core/qmlengine.h"
#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "workspace/workspace.h"

#include <woutputrenderwindow.h>

SessionSwitcher::SessionSwitcher(Workspace *workspace, QmlEngine *engine, WaylandServer *runtime,
                                 DesktopEnvironment *desktop)
    : QObject(desktop)
    , m_workspace(workspace)
    , m_engine(engine)
    , m_runtime(runtime)
    , m_desktop(desktop)
{
}

void SessionSwitcher::setShortcutController(ShortcutController *controller, ShortcutManagerV3 *protocol)
{
    Q_ASSERT(!m_shortcuts);
    m_shortcuts = new ShortcutRunner(m_workspace, this, controller, m_runtime, m_inputFilter, protocol);
    if (m_running)
        m_shortcuts->start();
}

QQuickItem *SessionSwitcher::taskSwitcher() const
{
    return m_taskSwitcher;
}

QQuickItem *SessionSwitcher::ensureTaskSwitcher(Output *output)
{
    if (!m_taskSwitcher) {
        m_taskSwitcher = m_engine->createTaskSwitcher(
            output, m_desktop->viewRoot(), m_desktop);
        m_taskSwitcher->setZ(RootSurfaceContainer::OverlayZOrder);
        connect(m_taskSwitcher, SIGNAL(switchOnChanged()), this, SLOT(close()));
    }
    return m_taskSwitcher;
}

void SessionSwitcher::close()
{
    if (!m_taskSwitcher)
        return;
    disconnect(m_taskSwitcher, nullptr, this, nullptr);
    m_taskSwitcher->setVisible(false);
    m_taskSwitcher->deleteLater();
    m_taskSwitcher = nullptr;
    auto *server = m_runtime;
    if (server->currentMode() == WaylandServer::CurrentMode::WindowSwitch)
        server->setCurrentMode(WaylandServer::CurrentMode::Normal);
}

void SessionSwitcher::deactivate()
{
    m_running = false;
    if (m_shortcuts)
        m_shortcuts->stop();
    close();
}

void SessionSwitcher::activate()
{
    m_running = true;
    if (m_shortcuts)
        m_shortcuts->start();
}

void SessionSwitcher::setInputFilter(CompositorInputFilter *filter)
{
    Q_ASSERT(!m_shortcuts && !m_inputFilter);
    m_inputFilter = filter;
}

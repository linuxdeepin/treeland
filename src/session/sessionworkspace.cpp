// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessionworkspace.h"
#include "session.h"
#include "core/rootsurfacecontainer.h"
#include "core/waylandserver.h"
#include "workspace/workspace.h"

SessionWorkspace::SessionWorkspace(Session *session, WaylandServer *runtime)
    : SessionComponent(session)
    , m_workspace(new Workspace(runtime->rootSurfaceContainer(), session->config()))
{
    m_workspace->setZ(RootSurfaceContainer::NormalZOrder);
    m_workspace->setSize(runtime->rootSurfaceContainer()->size());
    deactivate();
    initializeActiveState();
}

SessionWorkspace::~SessionWorkspace()
{
    if (!m_workspace)
        return;

    deactivate();
    auto *runtime = WaylandServer::instance();
    // The runtime may still reference the last active workspace. In that case
    // its visual parent retains ownership, as it does during runtime shutdown.
    if (!runtime || runtime->workspace() == m_workspace)
        return;

    auto *workspace = m_workspace.data();
    if (workspace->surfaces().isEmpty()) {
        workspace->deleteLater();
    } else {
        connect(workspace, &SurfaceContainer::surfaceRemoved, workspace, [workspace] {
            if (workspace->surfaces().isEmpty())
                workspace->deleteLater();
        });
    }
}

Workspace *SessionWorkspace::workspace() const
{
    return m_workspace;
}

void SessionWorkspace::activate()
{
    m_workspace->setEnabled(true);
    m_workspace->setVisible(true);
}

void SessionWorkspace::deactivate()
{
    m_workspace->setEnabled(false);
    m_workspace->resetView();
    m_workspace->setVisible(false);
}

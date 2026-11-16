// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "sessioncomponent.h"

#include <wextworkspacemanagerv1.h>
#include <QHash>

class WaylandServer;
class Workspace;
class WorkspaceModel;
class RootSurfaceContainer;

class SessionWorkspace : public SessionComponent
{
    Q_OBJECT
public:
    SessionWorkspace(Session *session, WaylandServer *runtime);
    ~SessionWorkspace() override;

    Workspace *workspace() const;
    void installProtocol();

protected:
    void activate() override;
    void deactivate() override;

private:
    void initializeProtocol();
    void invalidateProtocol();
    void syncModels();
    void syncOutputs();
    void handleCommit(wlr_ext_workspace_v1_commit_event *event);

    RootSurfaceContainer *const m_root;
    QPointer<Workspace> m_workspace;
    QPointer<WAYLIB_SERVER_NAMESPACE::WExtWorkspaceManagerV1> m_protocol;
    wlr_ext_workspace_group_handle_v1 *m_group = nullptr;
    QHash<WorkspaceModel *, wlr_ext_workspace_handle_v1 *> m_handles;
    QList<QPointer<WAYLIB_SERVER_NAMESPACE::WOutput>> m_outputs;
    const QByteArray m_protocolIdPrefix;
};

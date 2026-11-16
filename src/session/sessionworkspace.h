// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "sessioncomponent.h"

class WaylandServer;
class Workspace;

class SessionWorkspace : public SessionComponent
{
    Q_OBJECT
public:
    SessionWorkspace(Session *session, WaylandServer *runtime);
    ~SessionWorkspace() override;

    Workspace *workspace() const;

protected:
    void activate() override;
    void deactivate() override;

private:
    QPointer<Workspace> m_workspace;
};

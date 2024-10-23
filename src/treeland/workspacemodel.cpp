// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "workspacemodel.h"

#include "helper.h"
#include "surfacewrapper.h"

WorkspaceModel::WorkspaceModel(QObject *parent,
                               int id,
                               std::forward_list<SurfaceWrapper *> activedSurfaceHistory)
    : SurfaceListModel(parent)
    , m_id(id)
    , m_activedSurfaceHistory(activedSurfaceHistory)
{
}

QString WorkspaceModel::name() const
{
    return m_name;
}

void WorkspaceModel::setName(const QString &newName)
{
    if (m_name == newName)
        return;
    m_name = newName;
    Q_EMIT nameChanged();
}

int WorkspaceModel::id() const
{
    return m_id;
}

bool WorkspaceModel::visible() const
{
    return m_visible;
}

void WorkspaceModel::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    for (auto surface : surfaces())
        surface->setVisible(visible);
    Q_EMIT visibleChanged();
}

bool WorkspaceModel::opaque() const
{
    return m_opaque;
}

void WorkspaceModel::setOpaque(bool opaque)
{
    if (m_opaque == opaque)
        return;
    m_opaque = opaque;
    for (auto surface : surfaces())
        surface->setOpacity(opaque ? 1.0 : 0.0);
    Q_EMIT opaqueChanged();
}

void WorkspaceModel::addSurface(SurfaceWrapper *surface)
{
    SurfaceListModel::addSurface(surface);
    if (!m_opaque) {
        surface->setOpacity(m_opaque ? 1.0 : 0.0);
    } else {
        surface->setVisible(m_visible);
    }
    surface->setWorkspaceId(m_id);
}

void WorkspaceModel::removeSurface(SurfaceWrapper *surface)
{
    SurfaceListModel::removeSurface(surface);
    surface->setWorkspaceId(-1);
    m_activedSurfaceHistory.remove(surface);
}

SurfaceWrapper *WorkspaceModel::latestActiveSurface() const
{
    if (m_activedSurfaceHistory.empty())
        return nullptr;
    return m_activedSurfaceHistory.front();
}

void WorkspaceModel::pushActivedSurface(SurfaceWrapper *surface)
{
    m_activedSurfaceHistory.push_front(surface);
}

void WorkspaceModel::removeActivedSurface(SurfaceWrapper *surface)
{
    m_activedSurfaceHistory.remove(surface);
}

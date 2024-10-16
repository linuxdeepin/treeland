// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "workspacemodel.h"

#include "helper.h"
#include "surfacewrapper.h"

WorkspaceModel::WorkspaceModel(QObject *parent, int index, std::list<SurfaceWrapper *> activedSurfaceHistory)
    : SurfaceListModel(parent)
    , m_index(index)
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

int WorkspaceModel::index() const
{
    return m_index;
}

void WorkspaceModel::setIndex(int newIndex)
{
    if (m_index == newIndex)
        return;
    m_index = newIndex;
    Q_EMIT indexChanged();
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

void WorkspaceModel::addSurface(SurfaceWrapper *surface)
{
    SurfaceListModel::addSurface(surface);
    surface->setVisible(m_visible);
    surface->setWorkspaceId(m_index);
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

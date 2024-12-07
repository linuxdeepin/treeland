// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surfacefilterproxymodel.h"
#include "workspacemodel.h"
#include "surfacewrapper.h"

SurfaceFilterProxyModel::SurfaceFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void SurfaceFilterProxyModel::setFilterAppId(const QString &appid)
{
    m_filterAppId = appid;
    invalidateFilter();
}

int SurfaceFilterProxyModel::activeIndex()
{
    return m_activeIndex;
}

void SurfaceFilterProxyModel::setActiveIndex(int index)
{
    if (m_activeIndex != index) {
        m_activeIndex = index;
        Q_EMIT activeIndexChanged();
    }
}

bool SurfaceFilterProxyModel::filterAcceptsRow(int source_row,
                                               const QModelIndex &source_parent) const
{
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    SurfaceWrapper *surface = sourceModel()->data(index).value<SurfaceWrapper *>();
    auto wsurface = surface->shellSurface();
    Q_ASSERT(wsurface);

    if (m_filterAppId.isEmpty()) {
        return true;
    }

    if (surface) {
        return wsurface->appId() == m_filterAppId;
    }

    return false;
}

bool SurfaceFilterProxyModel::lessThan(const QModelIndex &source_left,
                                       const QModelIndex &source_right) const
{
    WorkspaceModel *model = dynamic_cast<WorkspaceModel *>(sourceModel());
    SurfaceWrapper *left_surface = sourceModel()->data(source_left).value<SurfaceWrapper *>();
    SurfaceWrapper *right_surface = sourceModel()->data(source_right).value<SurfaceWrapper *>();

    if (model && left_surface && right_surface) {
        int left_index = model->findActivedSurfaceHistoryIndex(left_surface);
        int right_index = model->findActivedSurfaceHistoryIndex(right_surface);

        if (left_index > right_index)
            return true;
        else
            return false;
    }

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

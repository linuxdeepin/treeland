// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surfacefilterproxymodel.h"

#include "common/treelandlogging.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspacemodel.h"

SurfaceFilterProxyModel::SurfaceFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void SurfaceFilterProxyModel::setFilterAppId(const QString &appid)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_filterAppId = appid;
    endFilterChange();
#else
    m_filterAppId = appid;
    invalidateFilter();
#endif
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

    if (!surface) {
        qCWarning(lcTlSurface) << "filterAcceptsRow: null surface at row" << source_row;
        return false;
    }

    if (!matchesAppIdFilter(surface)) {
        return false;
    }

    // Modal dialog handling (ref: KWin::TabBox::ClientModel::clientToAddToList)
    SurfaceWrapper *modal = surface->findModal();
    if (modal && modal != surface) {
        return !matchesAppIdFilter(modal);
    }

    return true;
}

bool SurfaceFilterProxyModel::matchesAppIdFilter(SurfaceWrapper *surface) const
{
    if (m_filterAppId.isEmpty())
        return true;
    if (surface->appId() == m_filterAppId)
        return true;
    return surface->modal() && surface->parentSurface()
        && surface->parentSurface()->appId() == m_filterAppId;
}

bool SurfaceFilterProxyModel::lessThan(const QModelIndex &source_left,
                                       const QModelIndex &source_right) const
{
    WorkspaceModel *model = dynamic_cast<WorkspaceModel *>(sourceModel());
    SurfaceWrapper *left = sourceModel()->data(source_left).value<SurfaceWrapper *>();
    SurfaceWrapper *right = sourceModel()->data(source_right).value<SurfaceWrapper *>();

    if (model && left && right) {
        if (model->laterActiveThan(left, right))
            return true;
        else
            return false;
    }

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

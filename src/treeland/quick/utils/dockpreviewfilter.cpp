// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dockpreviewfilter.h"

#include <wxdgsurface.h>
#include <qwxdgshell.h>

#include <algorithm>
#include <vector>

DockPreviewFilterProxyModel::DockPreviewFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

bool DockPreviewFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex modelIndex = this->sourceModel()->index(sourceRow, 0, sourceParent);
    auto v = sourceModel()->data(modelIndex, sourceModel()->roleNames().key("source"));

    if (v.isNull()) {
        return false;
    }

    auto find = std::ranges::find_if(m_surfaces, [v](Waylib::Server::WSurface *s) {
        return s == v.value<Waylib::Server::WSurface*>();
    });

    return find != m_surfaces.end();
}

void DockPreviewFilterProxyModel::clear()
{
    m_surfaces.clear();

    invalidateFilter();
}

void DockPreviewFilterProxyModel::append(Waylib::Server::WSurface *surface)
{
    connect(surface, &Waylib::Server::WSurface::destroyed, this, [this, surface] {
        remove(surface);
    });

    m_surfaces.push_back(surface);

    invalidateFilter();
}

void DockPreviewFilterProxyModel::remove(Waylib::Server::WSurface *surface)
{
    std::erase_if(m_surfaces, [surface](auto *s) {
        return s == surface;
    });

    invalidateFilter();
}

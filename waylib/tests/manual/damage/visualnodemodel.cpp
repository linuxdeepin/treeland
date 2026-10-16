// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "visualnodemodel.h"

VisualNodeModel::VisualNodeModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_roleNames = {
        {IdRole, "id"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {LocalXRole, "localX"},
        {LocalYRole, "localY"},
        {LocalWidthRole, "localWidth"},
        {LocalHeightRole, "localHeight"},
        {M11Role, "m11"},
        {M12Role, "m12"},
        {M21Role, "m21"},
        {M22Role, "m22"},
        {M13Role, "m13"},
        {M23Role, "m23"},
        {M33Role, "m33"},
        {DxRole, "dx"},
        {DyRole, "dy"},
        {XRole, "x"},
        {YRole, "y"},
        {WRole, "w"},
        {HRole, "h"},
        {OccludedRole, "occluded"},
        {CulledRole, "culled"},
        {VisibleRole, "visible"},
        {ColorRole, "color"},
        {IsBackdropRole, "isBackdrop"},
        {FullyOpaqueRole, "fullyOpaque"},
        {PaintOrderRole, "paintOrder"},
        {HasContentRole, "hasContent"},
    };
}

int VisualNodeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_nodes.size();
}

QVariant VisualNodeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_nodes.size())
        return {};
    const QVariantMap &node = m_nodes.at(index.row());
    auto it = m_roleNames.constFind(role);
    if (it == m_roleNames.cend())
        return {};
    return node.value(it.value());
}

QHash<int, QByteArray> VisualNodeModel::roleNames() const
{
    return m_roleNames;
}

void VisualNodeModel::setNodes(const QVector<QVariantMap> &nodes)
{
    if (nodes.size() == m_nodes.size()) {
        // Same count: update in place, emit dataChanged per row.
        for (int i = 0; i < nodes.size(); ++i) {
            m_nodes[i] = nodes.at(i);
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
        }
    } else {
        // Different count: full reset.
        beginResetModel();
        m_nodes = nodes;
        endResetModel();
    }
}

void VisualNodeModel::updateNode(int row, const QVariantMap &data)
{
    if (row < 0 || row >= m_nodes.size())
        return;
    m_nodes[row] = data;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

void VisualNodeModel::clear()
{
    beginResetModel();
    m_nodes.clear();
    endResetModel();
}

QVariantMap VisualNodeModel::getRow(int row) const
{
    if (row < 0 || row >= m_nodes.size())
        return {};
    return m_nodes.at(row);
}

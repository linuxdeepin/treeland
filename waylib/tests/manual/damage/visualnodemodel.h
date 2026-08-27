// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef VISUALNODEMODEL_H
#define VISUALNODEMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

class VisualNodeModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        LocalXRole,
        LocalYRole,
        LocalWidthRole,
        LocalHeightRole,
        M11Role,
        M12Role,
        M21Role,
        M22Role,
        M13Role,
        M23Role,
        M33Role,
        DxRole,
        DyRole,
        XRole,
        YRole,
        WRole,
        HRole,
        OccludedRole,
        CulledRole,
        VisibleRole,
        ColorRole,
        IsBackdropRole,
        FullyOpaqueRole,
        PaintOrderRole,
        HasContentRole,
    };
    Q_ENUM(Roles)

    explicit VisualNodeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setNodes(const QVector<QVariantMap> &nodes);
    void updateNode(int row, const QVariantMap &data);
    void clear();
    QVariantMap getRow(int row) const;

private:
    QVector<QVariantMap> m_nodes;
    QHash<int, QByteArray> m_roleNames;
};

#endif // VISUALNODEMODEL_H

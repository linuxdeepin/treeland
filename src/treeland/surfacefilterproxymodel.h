// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QSortFilterProxyModel>

class SurfaceFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit SurfaceFilterProxyModel(QObject *parent = nullptr);

    Q_INVOKABLE void setFilterAppId(const QString &appid);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    QString m_filterAppId;
};


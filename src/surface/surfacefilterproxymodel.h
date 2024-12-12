// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QSortFilterProxyModel>

class SurfaceFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY activeIndexChanged FINAL)

public:
    explicit SurfaceFilterProxyModel(QObject *parent = nullptr);

    Q_INVOKABLE void setFilterAppId(const QString &appid);

    int activeIndex();
    void setActiveIndex(int index);

Q_SIGNALS:
    void activeIndexChanged();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    QString m_filterAppId;
    mutable int m_activeIndex = -1;
};

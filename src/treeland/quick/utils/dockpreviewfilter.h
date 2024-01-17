// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QSortFilterProxyModel>
#include <QQmlEngine>

#include <wsurface.h>

class DockPreviewFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_NAMED_ELEMENT(DockPreviewFilter)

public:
    explicit DockPreviewFilterProxyModel(QObject *parent = nullptr);

    Q_INVOKABLE void clear();
    Q_INVOKABLE void append(Waylib::Server::WSurface *surface);
    Q_INVOKABLE void remove(Waylib::Server::WSurface *surface);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    std::vector<Waylib::Server::WSurface*> m_surfaces;
};

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "surfacecontainer.h"

class SurfaceWrapper;
class Workspace;
class WorkspaceModel : public SurfaceListModel
{
    friend class Workspace;
    Q_OBJECT
    Q_PROPERTY(int index READ index NOTIFY indexChanged FINAL)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged FINAL)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged FINAL)

    QML_ELEMENT

public:
    explicit WorkspaceModel(QObject *parent, int index);

    QString name() const;
    void setName(const QString &newName);

    int index() const;
    void setIndex(int newIndex);

    bool visible() const;
    void setVisible(bool visible);

    void addSurface(SurfaceWrapper *surface) override;
    void removeSurface(SurfaceWrapper *surface) override;

Q_SIGNALS:
    void nameChanged();
    void indexChanged();
    void visibleChanged();

private:
    QString m_name;
    int m_index = -1;
    bool m_visible = false;
};

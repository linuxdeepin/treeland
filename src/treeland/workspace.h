// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "surfacecontainer.h"

class SurfaceWrapper;
class Workspace;

class WorkspaceContainer : public SurfaceContainer
{
    friend class Workspace;
    Q_OBJECT
    Q_PROPERTY(int index READ index NOTIFY indexChanged FINAL)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged FINAL)
    QML_ANONYMOUS

public:
    explicit WorkspaceContainer(Workspace *parent, int index);

    QString name() const;
    void setName(const QString &newName);

    int index() const;
    void setIndex(int newIndex);

signals:
    void nameChanged();
    void indexChanged();

private:
    QString m_name;
    int m_index = -1;
};

class Workspace : public SurfaceContainer
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged FINAL)
    Q_PROPERTY(WorkspaceContainer* current READ current WRITE setCurrent NOTIFY currentChanged FINAL)
    QML_ANONYMOUS

public:
    explicit Workspace(SurfaceContainer *parent);

    void addSurface(SurfaceWrapper *surface, int workspaceIndex = -1);
    void removeSurface(SurfaceWrapper *surface) override;
    int containerIndexOfSurface(SurfaceWrapper *surface) const;

    int createContainer(const QString &name, bool visible = false);
    void removeContainer(int index);
    WorkspaceContainer *container(int index) const;

    int currentIndex() const;
    void setCurrentIndex(int newCurrentIndex);
    Q_INVOKABLE void switchToNext();
    Q_INVOKABLE void switchToPrev();
    void switchTo(int index);

    WorkspaceContainer *current() const;
    void setCurrent(WorkspaceContainer *container);

signals:
    void currentChanged();

private:
    void updateSurfaceOwnsOutput(SurfaceWrapper *surface);
    void updateSurfacesOwnsOutput();

    int m_currentIndex = 0;
    QList<WorkspaceContainer *> m_containers;
    QPointer<QQuickItem> m_switcher;
};

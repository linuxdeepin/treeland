// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "surfacecontainer.h"
#include "workspacemodel.h"

class SurfaceWrapper;
class Workspace;

class Workspace : public SurfaceContainer
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged FINAL)
    Q_PROPERTY(WorkspaceModel* current READ current WRITE setCurrent NOTIFY currentChanged FINAL)
    Q_PROPERTY(WorkspaceModel* showOnAllWorkspaceModel READ showOnAllWorkspaceModel CONSTANT)
    Q_PROPERTY(QQmlListProperty<WorkspaceModel> models READ models NOTIFY modelsChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    QML_ELEMENT

public:
    explicit Workspace(SurfaceContainer *parent);

    Q_INVOKABLE void addSurface(SurfaceWrapper *surface, int workspaceIndex = -1);
    void removeSurface(SurfaceWrapper *surface) override;
    int modelIndexOfSurface(SurfaceWrapper *surface) const;

    Q_INVOKABLE int createModel(const QString &name, bool visible = false);
    Q_INVOKABLE void removeModel(int index);
    WorkspaceModel *model(int index) const;

    int count() const;
    int currentIndex() const;
    WorkspaceModel *showOnAllWorkspaceModel() const;
    void setCurrentIndex(int newCurrentIndex);
    Q_INVOKABLE void switchToNext();
    Q_INVOKABLE void switchToPrev();
    void switchTo(int index);

    WorkspaceModel *current() const;
    void setCurrent(WorkspaceModel *container);

    QQmlListProperty<WorkspaceModel> models();
    static inline constexpr int ShowOnAllWorkspaceIndex = -2;

    Q_INVOKABLE void hideAllSurfacesExceptPreviewing(SurfaceWrapper *previewingItem);
    Q_INVOKABLE void showAllSurfaces();

Q_SIGNALS:
    void currentChanged();
    void countChanged();
    void modelsChanged();

private:
    void updateSurfaceOwnsOutput(SurfaceWrapper *surface);
    void updateSurfacesOwnsOutput();

    int m_currentIndex = 0;
    QList<WorkspaceModel *> m_models;
    WorkspaceModel *m_showOnAllWorkspaceModel;
    QPointer<QQuickItem> m_switcher;
};

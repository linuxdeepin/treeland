// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "surfacecontainer.h"
#include "workspacemodel.h"

class SurfaceWrapper;
class Workspace;
class WorkspaceAnimationController;
Q_MOC_INCLUDE("workspaceanimationcontroller.h")

class WorkspaceListModel : public ObjectListModel<WorkspaceModel>
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    explicit WorkspaceListModel(QObject *parent = nullptr);
    void moveModelTo(int workspaceIndex, int destinationIndex);
};

class Workspace : public SurfaceContainer
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged FINAL)
    Q_PROPERTY(WorkspaceModel* current READ current NOTIFY currentChanged FINAL)
    Q_PROPERTY(WorkspaceModel* showOnAllWorkspaceModel READ showOnAllWorkspaceModel CONSTANT)
    Q_PROPERTY(WorkspaceListModel *models READ models CONSTANT FINAL)
    Q_PROPERTY(WorkspaceAnimationController* animationController READ animationController CONSTANT FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    QML_ELEMENT

public:
    explicit Workspace(SurfaceContainer *parent);

    Q_INVOKABLE void moveSurfaceTo(SurfaceWrapper *surface, int workspaceIndex = -1);
    // When workspaceIndex is -1 will move to current workspace
    void addSurface(SurfaceWrapper *surface, int workspaceIndex = -1);
    Q_INVOKABLE void moveModelTo(int workspaceIndex, int destinationIndex);

    void removeSurface(SurfaceWrapper *surface) override;
    int modelIndexOfSurface(SurfaceWrapper *surface) const;

    Q_INVOKABLE int createModel(const QString &name, bool visible = false);
    Q_INVOKABLE void removeModel(int index);
    WorkspaceModel *model(int index) const;

    int count() const;
    int currentIndex() const;
    WorkspaceModel *showOnAllWorkspaceModel() const;
    void doSetCurrentIndex(int newCurrentIndex);
    void setCurrentIndex(int newCurrentIndex);
    Q_INVOKABLE void switchToNext();
    Q_INVOKABLE void switchToPrev();
    Q_INVOKABLE void switchTo(int index);

    WorkspaceModel *current() const;
    void setCurrent(WorkspaceModel *container);

    WorkspaceListModel *models();
    static inline constexpr int ShowOnAllWorkspaceIndex = -2;

    Q_INVOKABLE void hideAllSurfacesExceptPreviewing(SurfaceWrapper *previewingItem);
    Q_INVOKABLE void showAllSurfaces();

    void pushActivedSurface(SurfaceWrapper *surface);
    void removeActivedSurface(SurfaceWrapper *surface);
    void setSwitcherEnabled(bool enabled);

    WorkspaceAnimationController *animationController() const;

Q_SIGNALS:
    void currentChanged();
    void currentIndexChanged();
    void countChanged();

private:
    int doCreateModel(const QString &name, bool visible = false);
    void doRemoveModel(int index);
    void updateSurfaceOwnsOutput(SurfaceWrapper *surface);
    void updateSurfacesOwnsOutput();

    uint m_currentIndex;
    WorkspaceListModel *m_models;
    WorkspaceModel *m_showOnAllWorkspaceModel;
    QPointer<QQuickItem> m_switcher;
    WorkspaceAnimationController *m_animationController;
    bool m_switcherEnabled = true;
};

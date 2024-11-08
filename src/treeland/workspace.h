// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "surfacecontainer.h"
#include "surfacefilterproxymodel.h"
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
    bool moveRows(const QModelIndex &sourceParent,
                  int sourceRow,
                  int count,
                  const QModelIndex &destinationParent,
                  int destinationChild) override;
};

class Workspace : public SurfaceContainer
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged FINAL)
    Q_PROPERTY(WorkspaceModel *current READ current NOTIFY currentChanged FINAL)
    Q_PROPERTY(SurfaceFilterProxyModel* currentFilter READ currentFilter NOTIFY currentFilterChanged FINAL)
    Q_PROPERTY(WorkspaceModel *showOnAllWorkspaceModel READ showOnAllWorkspaceModel CONSTANT FINAL)
    Q_PROPERTY(WorkspaceListModel *models READ models CONSTANT FINAL)
    Q_PROPERTY(WorkspaceAnimationController *animationController READ animationController CONSTANT FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    QML_ELEMENT

public:
    explicit Workspace(SurfaceContainer *parent);

    // When workspaceId is -1 will move to current workspace
    Q_INVOKABLE void moveSurfaceTo(SurfaceWrapper *surface, int workspaceId = -1);
    Q_INVOKABLE int getLeftWorkspaceId(int workspaceId);
    Q_INVOKABLE int getRightWorkspaceId(int workspaceId);

    void addSurface(SurfaceWrapper *surface, int workspaceId);
    void addSurface(SurfaceWrapper *surface) override;
    Q_INVOKABLE void moveModelTo(int workspaceId, int destinationIndex);

    void removeSurface(SurfaceWrapper *surface) override;
    int modelIndexOfSurface(SurfaceWrapper *surface) const;

    Q_INVOKABLE int createModel(const QString &name = QLatin1String(""), bool visible = false);
    Q_INVOKABLE void removeModel(int index);
    Q_INVOKABLE WorkspaceModel *modelAt(int index) const;
    Q_INVOKABLE WorkspaceModel *modelFromId(int id) const;

    int count() const;
    int currentIndex() const;
    WorkspaceModel *showOnAllWorkspaceModel() const;
    void setCurrentIndex(int newCurrentIndex);
    Q_INVOKABLE void switchToNext();
    Q_INVOKABLE void switchToPrev();
    Q_INVOKABLE void switchTo(int index);

    WorkspaceModel *current() const;
    void setCurrent(WorkspaceModel *container);

    SurfaceFilterProxyModel *currentFilter();

    WorkspaceListModel *models();
    static inline constexpr int ShowOnAllWorkspaceId = -2;

    Q_INVOKABLE void startPreviewing(SurfaceWrapper *previewingItem);
    Q_INVOKABLE void stopPreviewing();

    void pushActivedSurface(SurfaceWrapper *surface);
    void removeActivedSurface(SurfaceWrapper *surface);
    void setSwitcherEnabled(bool enabled);

    WorkspaceAnimationController *animationController() const;
    void createSwitcher();

Q_SIGNALS:
    void currentChanged();
    void currentFilterChanged();
    void currentIndexChanged();
    void countChanged();

private:
    int nextWorkspaceId() const;
    int doCreateModel(const QString &name, bool visible = false);
    void doRemoveModel(int index);
    void doSetCurrentIndex(int newCurrentIndex);
    void updateSurfaceOwnsOutput(SurfaceWrapper *surface);
    void updateSurfacesOwnsOutput();

    uint m_currentIndex;
    WorkspaceListModel *m_models;
    WorkspaceModel *m_showOnAllWorkspaceModel;
    QPointer<QQuickItem> m_switcher;
    SurfaceFilterProxyModel *m_currentFilter;
    WorkspaceAnimationController *m_animationController;
    bool m_switcherEnabled = true;
    QPointer<SurfaceWrapper> m_previewingItem;
};

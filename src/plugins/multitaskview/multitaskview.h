// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "multitaskviewinterface.h"

#include <QAbstractListModel>
#include <QQuickItem>
Q_MOC_INCLUDE("surfacecontainer.h")
Q_MOC_INCLUDE("workspacemodel.h")
Q_MOC_INCLUDE("output.h")
class SurfaceListModel;
class SurfaceWrapper;
class WorkspaceModel;
class Output;

class Multitaskview : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Status status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(ActiveReason activeReason READ activeReason NOTIFY activeReasonChanged FINAL)

public:
    enum Status
    {
        Uninitialized,
        Initialized,
        Active,
        Exited
    };
    Q_ENUM(Status)

    enum ActiveReason
    {
        ShortcutKey = IMultitaskView::ShortcutKey,
        Gesture = IMultitaskView::Gesture,
    };
    Q_ENUM(ActiveReason)

    enum ZOrder
    {
        Background = -1,
        Overlay = 1,
        FloatingItem = 2
    };
    Q_ENUM(ZOrder)

    Multitaskview(QQuickItem *parent = nullptr);

    Status status() const;
    void setStatus(Status status);
    ActiveReason activeReason() const;
    void setActiveReason(ActiveReason activeReason);

Q_SIGNALS:
    void statusChanged();
    void activeReasonChanged();
    void aboutToExit(); // Focus has been updated, waiting for exit

public Q_SLOTS:
    void exit(SurfaceWrapper *surface = nullptr, bool immediately = false);
    void enter(ActiveReason reason);

private:
    Status m_status;
    ActiveReason m_activeReason;
};

class MultitaskviewSurfaceModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(WorkspaceModel* workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged REQUIRED FINAL)
    Q_PROPERTY(QRectF layoutArea READ layoutArea WRITE setLayoutArea NOTIFY layoutAreaChanged REQUIRED FINAL)
    Q_PROPERTY(bool modelReady READ modelReady NOTIFY modelReadyChanged FINAL)
    Q_PROPERTY(uint rows READ rows NOTIFY rowsChanged FINAL)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentHeightChanged FINAL)
    Q_PROPERTY(Output *output READ output WRITE setOutput NOTIFY outputChanged FINAL)
    Q_PROPERTY(uint count READ count NOTIFY countChanged FINAL)

    struct SurfaceModelData
    {
        SurfaceWrapper *wrapper{ nullptr };
        QRectF geometry{};
        bool padding{ false };
        bool minimized{ false };
        int upIndex{ 0 };
        int downIndex{ 0 };
        int leftIndex{ 0 };
        int rightIndex{ 0 };

        QRectF pendingGeometry{};
        bool pendingPadding{ false };
        int zorder{ 0 };
        int pendingUpIndex;
        int pendingDownIndex;
        int pendingLeftIndex;
        int pendingRightIndex;

        void commit()
        {
            geometry = pendingGeometry;
            padding = pendingPadding;
            upIndex = pendingUpIndex;
            downIndex = pendingDownIndex;
            leftIndex = pendingLeftIndex;
            rightIndex = pendingRightIndex;
        }
    };

    using ModelDataPtr = std::shared_ptr<SurfaceModelData>;

public:
    MultitaskviewSurfaceModel(QObject *parent = nullptr);
    void initializeModel();

    enum SurfaceModelRole
    {
        SurfaceWrapperRole = Qt::UserRole + 1,
        GeometryRole,
        PaddingRole,
        ZOrderRole,
        MinimizedRole,
        UpIndexRole,
        DownIndexRole,
        LeftIndexRole,
        RightIndexRole
    };
    Q_ENUM(SurfaceModelRole)

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QModelIndex index(int row,
                      int column = 0,
                      const QModelIndex &parent = QModelIndex()) const override;
    bool modelReady() const;

    Q_INVOKABLE void calcLayout();
    Q_INVOKABLE void updateZOrder();
    Q_INVOKABLE int prevSameAppIndex(int index);
    Q_INVOKABLE int nextSameAppIndex(int index);

    QRectF layoutArea() const;
    void setLayoutArea(const QRectF &newLayoutArea);

    uint rows() const;
    void setRows(uint newRows);

    WorkspaceModel *workspace() const;
    void setWorkspace(WorkspaceModel *newWorkspace);

    qreal contentHeight() const;

    Output *output() const;
    void setOutput(Output *newOutput);

    uint count() const;

Q_SIGNALS:
    void surfaceListModelChanged();
    void layoutAreaChanged();
    void modelReadyChanged();
    void rowsChanged();
    void workspaceChanged();
    void contentHeightChanged();
    void outputChanged();
    void countChanged();

private:
    bool tryLayout(const QList<ModelDataPtr> &rawData, qreal rowH, bool ignoreOverlap = false);
    void calcDisplayPos(const QList<ModelDataPtr> &rawData);
    void doCalculateLayout(const QList<ModelDataPtr> &rawData);
    void doUpdateZOrder(const QList<ModelDataPtr> &rawData);
    std::pair<int, int> commitAndGetUpdateRange(const QList<ModelDataPtr> &rawData);
    void handleWrapperGeometryChanged();
    void handleWrapperOutputChanged();
    void handleSurfaceStateChanged();
    void handleSurfaceMappedChanged();
    void handleSurfaceAdded(SurfaceWrapper *surface);
    void handleSurfaceRemoved(SurfaceWrapper *surface);
    void addReadySurface(SurfaceWrapper *surface);
    void monitorUnreadySurface(SurfaceWrapper *surface);
    bool surfaceReady(SurfaceWrapper *surface);
    QRectF surfaceGeometry(SurfaceWrapper *surface);
    bool laterActiveThan(SurfaceWrapper *a, SurfaceWrapper *b);
    void connectWorkspace(WorkspaceModel *workspace);
    void disconnectWorkspace(WorkspaceModel *workspace);

    QList<ModelDataPtr> m_data{};
    QRectF m_layoutArea{};
    QList<QList<ModelDataPtr>> m_rows{};
    qreal m_rowHeight{ 0 };
    qreal m_contentHeight{ 0 };
    bool m_modelReady;
    QList<ModelDataPtr> m_toBeInserted;
    WorkspaceModel *m_workspace = nullptr;
    Output *m_output = nullptr;
};

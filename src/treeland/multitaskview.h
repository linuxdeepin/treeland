// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QAbstractListModel>
#include <QQuickItem>
Q_MOC_INCLUDE("surfacecontainer.h")
Q_MOC_INCLUDE("workspacemodel.h")
class SurfaceListModel;
class SurfaceWrapper;
class WorkspaceModel;

class Multitaskview : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Status status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(ActiveReason activeReason READ activeReason NOTIFY activeReasonChanged FINAL)
    Q_PROPERTY(qreal taskviewVal READ taskviewVal WRITE setTaskviewVal NOTIFY taskviewValChanged FINAL)

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
        ShortcutKey = 1,
        Gesture
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
    qreal taskviewVal() const;
    void setTaskviewVal(qreal taskviewVal);

Q_SIGNALS:
    void statusChanged();
    void activeReasonChanged();
    void taskviewValChanged();

public Q_SLOTS:
    void exit(SurfaceWrapper *surface = nullptr);
    void enter(ActiveReason reason);

private:
    Status m_status;
    ActiveReason m_activeReason;
    qreal m_taskviewVal;
};

class MultitaskviewSurfaceModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(WorkspaceModel* workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged REQUIRED FINAL)
    Q_PROPERTY(QRectF layoutArea READ layoutArea WRITE setLayoutArea NOTIFY layoutAreaChanged REQUIRED FINAL)
    Q_PROPERTY(bool modelReady READ modelReady NOTIFY modelReadyChanged FINAL)
    Q_PROPERTY(uint rows READ rows NOTIFY rowsChanged FINAL)

    static inline constexpr qreal LoadFactor = 0.6;
    static inline constexpr qreal CellPadding = 12;
    static inline constexpr qreal TopContentMargin = 40;

    struct SurfaceModelData
    {
        SurfaceWrapper *wrapper{ nullptr };
        QRectF geometry{};
        bool padding{ false };
        QRectF pendingGeometry{};
        bool pendingPadding{ false };
        int zorder{ 0 };
        int pendingZOrder{ 0 };

        void commit()
        {
            geometry = pendingGeometry;
            padding = pendingPadding;
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
        ZOrderRole
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

    QRectF layoutArea() const;
    void setLayoutArea(const QRectF &newLayoutArea);

    uint rows() const;
    void setRows(uint newRows);

    WorkspaceModel *workspace() const;
    void setWorkspace(WorkspaceModel *newWorkspace);

Q_SIGNALS:
    void surfaceListModelChanged();
    void layoutAreaChanged();
    void modelReadyChanged();
    void rowsChanged();
    void workspaceChanged();

private:
    bool tryLayout(const QList<ModelDataPtr> &rawData, qreal rowH, bool ignoreOverlap = false);
    void calcDisplayPos();
    void doCalculateLayout(const QList<ModelDataPtr> &rawData);
    void doUpdateZOrder(const QList<ModelDataPtr> &rawData);
    std::pair<int, int> commitAndGetUpdateRange(const QList<ModelDataPtr> &rawData);
    void handleWrapperGeometryChanged();
    void handleSurfaceMappedChanged();
    void handleSurfaceAdded(SurfaceWrapper *surface);
    void handleSurfaceRemoved(SurfaceWrapper *surface);
    void addReadySurface(SurfaceWrapper *surface);
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
};

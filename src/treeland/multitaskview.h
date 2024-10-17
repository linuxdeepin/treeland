// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QAbstractListModel>
#include <QQuickItem>
Q_MOC_INCLUDE("surfacecontainer.h")

class SurfaceListModel;
class SurfaceWrapper;

class Multitaskview : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Status status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(ActiveReason activeReason READ activeReason NOTIFY activeReasonChanged FINAL)
    Q_PROPERTY(qreal taskviewVal READ taskviewVal WRITE setTaskviewVal NOTIFY taskviewValChanged FINAL)

public:
    enum Status { Uninitialized, Initialized, Active, Exited };
    Q_ENUM(Status)

    enum ActiveReason { ShortcutKey = 1, Gesture };
    Q_ENUM(ActiveReason)

    enum ZOrder { Background = -1, Overlay = 1, FloatingItem = 2 };
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
    Q_PROPERTY(SurfaceListModel *surfaceListModel READ surfaceListModel WRITE setSurfaceListModel NOTIFY surfaceListModelChanged REQUIRED FINAL)
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
    };

public:
    MultitaskviewSurfaceModel(QObject *parent = nullptr);
    void initializeModel();

    enum SurfaceModelRole { SurfaceWrapperRole = Qt::UserRole + 1, GeometryRole, PaddingRole };
    Q_ENUM(SurfaceModelRole)

    void setSurfaceListModel(SurfaceListModel *surfaceListModel);
    SurfaceListModel *surfaceListModel() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QModelIndex index(int row,
                      int column = 0,
                      const QModelIndex &parent = QModelIndex()) const override;
    bool modelReady() const;

    Q_INVOKABLE void calcLayout();

    QRectF layoutArea() const;
    void setLayoutArea(const QRectF &newLayoutArea);

    uint rows() const;
    void setRows(uint newRows);

Q_SIGNALS:
    void surfaceListModelChanged();
    void layoutAreaChanged();
    void modelReadyChanged();

    void rowsChanged();

private:
    bool tryLayout(qreal rowH, bool ignoreOverlap = false);
    void calcDisplayPos();

    SurfaceListModel *m_surfaceListModel{ nullptr };
    QList<SurfaceModelData> m_data{};
    QRectF m_layoutArea{};
    QList<QList<SurfaceModelData *>> m_rows{};
    qreal m_rowHeight{ 0 };
    qreal m_contentHeight{ 0 };
    bool m_modelReady;
};

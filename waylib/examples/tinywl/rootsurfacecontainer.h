// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "surfacecontainer.h"

#include <wglobal.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
class WSurfaceItem;
class WToplevelSurface;
class WOutputLayout;
class WCursor;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class OutputListModel : public ObjectListModel<Output>
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit OutputListModel(QObject *parent = nullptr);
};

class RootSurfaceContainer : public SurfaceContainer
{
    Q_OBJECT
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WOutputLayout *outputLayout READ outputLayout CONSTANT FINAL)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WCursor *cursor READ cursor CONSTANT FINAL)
    Q_PROPERTY(Output *primaryOutput READ primaryOutput WRITE setPrimaryOutput NOTIFY primaryOutputChanged FINAL)
    Q_PROPERTY(OutputListModel* outputModel READ outputModel CONSTANT FINAL)

public:
    explicit RootSurfaceContainer(QQuickItem *parent);

    enum ContainerZOrder {
        BackgroundZOrder = -2,
        BottomZOrder = -1,
        NormalZOrder = 0,
        TopZOrder = 1,
        OverlayZOrder = 2,
        TaskBarZOrder = 3,
        MenuBarZOrder = 3,
        PopupZOrder = 4,
    };

    void init(WServer *server);

    SurfaceWrapper *getSurface(WSurface *surface) const;
    SurfaceWrapper *getSurface(WToplevelSurface *surface) const;
    void destroyForSurface(WSurface *surface);

    WOutputLayout *outputLayout() const;
    WCursor *cursor() const;

    Output *cursorOutput() const;
    Output *primaryOutput() const;
    void setPrimaryOutput(Output *newPrimaryOutput);
    const QList<Output*> &outputs() const;

    void addOutput(Output *output) override;
    void removeOutput(Output *output) override;

    void beginMoveResize(SurfaceWrapper *surface, Qt::Edges edges);
    void doMoveResize(const QPointF &incrementPos);
    void endMoveResize();
    SurfaceWrapper *moveResizeSurface() const;

    OutputListModel *outputModel() const;

public slots:
    void startMove(SurfaceWrapper *surface);
    void startResize(SurfaceWrapper *surface, Qt::Edges edges);
    void cancelMoveResize(SurfaceWrapper *surface);

signals:
    void primaryOutputChanged();
    void moveResizeFinised();

private:
    void addSurface(SurfaceWrapper *surface) override;
    void removeSurface(SurfaceWrapper *surface) override;

    void addBySubContainer(SurfaceContainer *, SurfaceWrapper *surface) override;
    void removeBySubContainer(SurfaceContainer *, SurfaceWrapper *surface) override;

    bool filterSurfaceGeometryChanged(SurfaceWrapper *surface, QRectF &newGeometry, const QRectF &oldGeometry) override;
    bool filterSurfaceStateChange(SurfaceWrapper *surface, SurfaceWrapper::State newState, SurfaceWrapper::State oldState) override;

    void ensureCursorVisible();
    void updateSurfaceOutputs(SurfaceWrapper *surface);
    void ensureSurfaceNormalPositionValid(SurfaceWrapper *surface);

    WOutputLayout *m_outputLayout = nullptr;
    OutputListModel *m_outputModel = nullptr;
    QPointer<Output> m_primaryOutput;
    WCursor *m_cursor = nullptr;
    WSurfaceItem *m_dragSurfaceItem = nullptr;

    // for move resize
    struct {
        SurfaceWrapper *surface = nullptr;
        QRectF startGeometry;
        Qt::Edges resizeEdges;
        bool setSurfacePositionForAnchorEdgets = false;
    } moveResizeState;
};

Q_DECLARE_OPAQUE_POINTER(WAYLIB_SERVER_NAMESPACE::WOutputLayout*)
Q_DECLARE_OPAQUE_POINTER(WAYLIB_SERVER_NAMESPACE::WCursor*)

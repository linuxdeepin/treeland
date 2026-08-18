// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <memory>
#include "surface/surfacecontainer.h"
#include "surface/seatsurfacemanager.h"

#include <wglobal.h>

Q_MOC_INCLUDE(<wcursor.h>)

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
class WSurfaceItem;
class WToplevelSurface;
class WOutputLayout;
class WCursor;
class WSeat;
class WInputDevice;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class Output;

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
    ~RootSurfaceContainer() override;

    enum ContainerZOrder
    {
        BackgroundZOrder = -2,
        BottomZOrder = -1,
        NormalZOrder = 0,
        MultitaskviewZOrder = 1,
        TopZOrder = 2,
        OverlayZOrder = 3,
        TaskBarZOrder = 4,
        MenuBarZOrder = 4,
        PopupZOrder = 5,
        CaptureLayerZOrder = 6,
        LockScreenZOrder = 7,
        GlobalOverlayZOrder = 100,
        PrivilegedOverlayZOrder = 200, // Privileged overlay, above lock screen & lock screen popups
    };

    void init(WServer *server);

    SurfaceWrapper *getSurface(WSurface *surface) const;
    SurfaceWrapper *getSurface(WToplevelSurface *surface) const;
    void destroyForSurface(SurfaceWrapper *wrapper);

    SeatSurfaceManager *getSeatContainer(WSeat *seat) const;
    WSeat *getDefaultSeat() const;
    SeatSurfaceManager *getSeatContainerOrDefault(WSeat *seat = nullptr) const;

    void beginMoveResizeForSeat(WSeat *seat, SurfaceWrapper *surface, Qt::Edges edges);
    void doMoveResizeForSeat(WSeat *seat, const QPointF &delta);
    void detectEdgeTilingForSeat(WSeat *seat);
    void endMoveResizeForSeat(WSeat *seat);
    void cancelMoveResizeForSeat(WSeat *seat);
    SurfaceWrapper *getMoveResizeSurfaceForSeat(WSeat *seat) const;
    bool isInMoveResizeForSeat(WSeat *seat) const;
    void setActivatedSurfaceForSeat(WSeat *seat, SurfaceWrapper *surface,
                                    Qt::FocusReason reason);
    SurfaceWrapper *getActivatedSurfaceForSeat(WSeat *seat) const;
    void setupSeatManagement();
    void dismissAllPopups();
    void givePopupFocus(SurfaceWrapper *popupWrapper);

    WOutputLayout *outputLayout() const;
    WCursor *cursor() const;

    Output *cursorOutput() const;
    Output *outputAt(const QPointF &pos) const;
    void updateEdgeTilePreview(QuickTile::Mode mode, Output *out);
    Output *primaryOutput() const;
    void setPrimaryOutput(Output *newPrimaryOutput, bool updateDconfig = false);
    const QList<Output *> &outputs() const;

    void addOutput(Output *output) override;
    void removeOutput(Output *output) override;

    // TODO(Lyn): These global move/resize interfaces should eventually be moved into the Seat
    //       object (or SeatSurfaceManager), since move/resize state is inherently per-seat.
    void beginMoveResize(SurfaceWrapper *surface, Qt::Edges edges);
    void doMoveResize(const QPointF &incrementPos);
    void endMoveResize();
    SurfaceWrapper *moveResizeSurface() const;
    bool isInMoveResize() const;

    OutputListModel *outputModel() const;
    void moveSurfacesToOutput(const QList<SurfaceWrapper *> &surfaces,
                              Output *targetOutput,
                              Output *sourceOutput = nullptr);
    void ensureSurfaceNormalPositionValid(SurfaceWrapper *surface);

public Q_SLOTS:
    void startMove(SurfaceWrapper *surface);
    void startResize(SurfaceWrapper *surface, Qt::Edges edges);
    void cancelMoveResize(SurfaceWrapper *surface);

Q_SIGNALS:
    void primaryOutputChanged();
    void moveResizeFinised();

private:
    void addSurface(SurfaceWrapper *surface) override;
    void removeSurface(SurfaceWrapper *surface) override;

    void addBySubContainer(SurfaceContainer *, SurfaceWrapper *surface) override;
    void removeBySubContainer(SurfaceContainer *, SurfaceWrapper *surface) override;

    bool filterSurfaceGeometryChanged(SurfaceWrapper *surface,
                                      QRectF &newGeometry,
                                      const QRectF &oldGeometry) override;
    bool filterSurfaceStateChange(SurfaceWrapper *surface,
                                  [[maybe_unused]] SurfaceWrapper::State newState,
                                  [[maybe_unused]] SurfaceWrapper::State oldState) override;

    void ensureCursorVisible();
    void updateSurfaceOutputs(SurfaceWrapper *surface);
    QQuickItem *ensureEdgeTilePreview();
    void onSeatAdded(WSeat *seat);
    void onSeatRemoved(WSeat *seat);

    void setupSurfaceRequestHandlers(SurfaceWrapper *surface);
    WSeat *determineSeatForRequest(SurfaceWrapper *surface);

    WOutputLayout *m_outputLayout = nullptr;
    std::unique_ptr<WAYLIB_SERVER_NAMESPACE::WListenerOwner> m_outputLayoutListenerOwner;
    OutputListModel *m_outputModel = nullptr;
    QPointer<Output> m_primaryOutput;
    WCursor *m_cursor = nullptr;
    WSurfaceItem *m_dragSurfaceItem = nullptr;
    QPointer<QQuickItem> m_edgeTilePreview;

    // Per-seat state management
    QMap<WSeat*, SeatSurfaceManager*> m_seatContainers;
};

Q_DECLARE_OPAQUE_POINTER(WAYLIB_SERVER_NAMESPACE::WOutputLayout *)

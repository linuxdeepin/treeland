// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surface/surfacewrapper.h"
#include "surface/quicktile.h"
#include <wseat.h>
#include <wscoplistener.h>
#include <QQuickItem>
#include <QMap>

class QTimer;

class RootSurfaceContainer;
class Output;
class Helper;

class SeatSurfaceManager : public QObject
{
    Q_OBJECT

public:
    explicit SeatSurfaceManager(WSeat *seat, RootSurfaceContainer *parent);
    ~SeatSurfaceManager() override;

    WSeat *seat() const { return m_seat; }
    RootSurfaceContainer *rootContainer() const { return m_rootContainer; }
    SurfaceWrapper *activatedSurface() const { return m_activatedSurface; }
    void setActivatedSurface(SurfaceWrapper *surface, Qt::FocusReason reason);
    SurfaceWrapper *keyboardFocusSurface() const { return m_keyboardFocusSurface; }
    void setKeyboardFocusSurface(SurfaceWrapper *surface, Qt::FocusReason reason = Qt::OtherFocusReason);

    void restoreShowDesktopFocus();

    struct MoveResizeState {
        SurfaceWrapper *surface = nullptr;  ///< The surface being moved/resized
        Qt::Edges edges = Qt::Edges();      ///< Resize edges (empty for move)
        QRectF startGeometry;               ///< Geometry at start of move/resize
        QPointF initialPosition;            ///< Initial cursor position
        bool settingPositionFlag = false;   ///< Flag to prevent recursive updates
        QuickTile::Mode detectedTileMode =
            QuickTile::Mode::None;          ///< Edge-tiling mode detected during move
        bool edgeTilePreviewActive = false; ///< Whether the edge-tiling preview is activated
        bool edgeTileInnerBorder = false;   ///< Whether the detected edge is shared with an adjacent output (inner edge)
        Output *detectedTileOutput = nullptr;  ///< The output on which the edge was detected (for cross-screen preview updates)
    };

    MoveResizeState &moveResizeState() { return m_moveResizeState; }
    const MoveResizeState &moveResizeState() const { return m_moveResizeState; }
    void beginMoveResize(SurfaceWrapper *surface, Qt::Edges edges);
    void doMoveResize(const QPointF &delta);
    void endMoveResize();
    SurfaceWrapper *moveResizeSurface() const;
    void cancelMoveResize(SurfaceWrapper *surface);
    void cancelMoveResize();
    void startEdgeTileDelay();
    void stopEdgeTileDelay();
    bool shouldHandleShortcuts() const;
    bool metaKeyPressed() const { return m_metaKeyPressed; }
    void setMetaKeyPressed(bool pressed);
    void surfaceDestroyed(SurfaceWrapper *surface);

    // Popup keyboard grab management
    void givePopupFocus(SurfaceWrapper *popupWrapper);
    void dismissPopups();
    bool hasPopupGrab() const { return m_popupKeyboardGrab != nullptr; }

Q_SIGNALS:
    void activatedSurfaceChanged(SurfaceWrapper *surface);
    void moveResizeChanged();

private:
    void onActivatedSurfaceFocusCapabilityChanged();
    void onKeyboardGrabBegin(wlr_seat_keyboard_grab *grab);
    void onKeyboardGrabEnd(wlr_seat_keyboard_grab *grab);
    SurfaceWrapper *popupParentFocusTarget(SurfaceWrapper *popup, bool skipPopupParents) const;
    bool isTrackedPopup(SurfaceWrapper *surface) const;

    WSeat *m_seat = nullptr;
    RootSurfaceContainer *m_rootContainer = nullptr;

    // Per-seat state
    SurfaceWrapper *m_activatedSurface = nullptr;
    SurfaceWrapper *m_keyboardFocusSurface = nullptr;
    MoveResizeState m_moveResizeState;
    bool m_metaKeyPressed = false;

    // Popup grab state
    wlr_seat_keyboard_grab *m_popupKeyboardGrab = nullptr;
    QPointer<SurfaceWrapper> m_prePopupFocusSurface;
    QList<QPointer<SurfaceWrapper>> m_popupFocusStack;
    quint64 m_popupTransitionSerial = 0;
    QTimer *m_edgeTileDelayTimer = nullptr;

    // Equivalent to the old QObject::connect on qw_seat; disconnect in the
    // destructor so wlr_seat_destroy does not assert on leftover listeners
    // when the seat is deleted before this object's deleteLater runs.
    WAYLIB_SERVER_NAMESPACE::WScopedListener keyboardGrabBeginListener;
    WAYLIB_SERVER_NAMESPACE::WScopedListener keyboardGrabEndListener;
};

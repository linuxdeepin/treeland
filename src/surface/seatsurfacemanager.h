// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surface/surfacewrapper.h"
#include <wseat.h>
#include <QQuickItem>
#include <QMap>

class RootSurfaceContainer;
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
    void setKeyboardFocusSurface(SurfaceWrapper *surface);

    struct MoveResizeState {
        SurfaceWrapper *surface = nullptr;  ///< The surface being moved/resized
        Qt::Edges edges = Qt::Edges();      ///< Resize edges (empty for move)
        QRectF startGeometry;               ///< Geometry at start of move/resize
        QPointF initialPosition;            ///< Initial cursor position
        bool settingPositionFlag = false;   ///< Flag to prevent recursive updates
    };

    MoveResizeState &moveResizeState() { return m_moveResizeState; }
    const MoveResizeState &moveResizeState() const { return m_moveResizeState; }
    void beginMoveResize(SurfaceWrapper *surface, Qt::Edges edges);
    void doMoveResize(const QPointF &delta);
    void endMoveResize();
    SurfaceWrapper *moveResizeSurface() const;
    void cancelMoveResize(SurfaceWrapper *surface);
    bool shouldHandleShortcuts() const;
    bool metaKeyPressed() const { return m_metaKeyPressed; }
    void setMetaKeyPressed(bool pressed);
    void surfaceDestroyed(SurfaceWrapper *surface);

Q_SIGNALS:
    void activatedSurfaceChanged(SurfaceWrapper *surface);
    void moveResizeChanged();

private:
    WSeat *m_seat = nullptr;
    RootSurfaceContainer *m_rootContainer = nullptr;
    
    // Per-seat state
    SurfaceWrapper *m_activatedSurface = nullptr;
    SurfaceWrapper *m_keyboardFocusSurface = nullptr;
    MoveResizeState m_moveResizeState;
    bool m_metaKeyPressed = false;
};

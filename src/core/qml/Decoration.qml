// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Waylib.Server
import Treeland

Item {
    id: root

    required property SurfaceWrapper surface

    readonly property real edgeSize: 10

    visible: surface && surface.visibleDecoration && surface.visible
    x: shadow.boundingRect.x
    y: shadow.boundingRect.y
    width: shadow.boundingRect.width
    height: shadow.boundingRect.height

    MouseArea {
        id: resizeArea

        readonly property XWaylandSurface xwaylandSurface: root.surface.shellSurface as XWaylandSurface

        containmentMask: FrameInputMask {
            id: resizeMask
            geometry: root.surface.frameGeometry
            frameItem: root.surface.frameItem
            target: resizeArea
            resizeMargin: root.edgeSize
        }

        // Do not manufacture a frame around an arbitrary X11 input shape.
        enabled: surface.isResizable && !surface.prelaunchSplash
                    && !(xwaylandSurface && xwaylandSurface.hasInputShape)
                    && surface.type !== SurfaceWrapper.Type.XdgPopup
                    && surface.type !== SurfaceWrapper.Type.Layer
                    && surface.type !== SurfaceWrapper.Type.SplashScreen
        property int edges: 0

        anchors {
            fill: shadow
            margins: -root.edgeSize
        }

        hoverEnabled: true
        Cursor.shape: {
            switch(edges) {
            case Qt.TopEdge:
                return CursorShape.TopSide
            case Qt.RightEdge:
                return CursorShape.RightSide
            case Qt.BottomEdge:
                return CursorShape.BottomSide
            case Qt.LeftEdge:
                return CursorShape.LeftSide
            case Qt.TopEdge | Qt.LeftEdge:
                return CursorShape.TopLeftCorner
            case Qt.TopEdge | Qt.RightEdge:
                return CursorShape.TopRightCorner
            case Qt.BottomEdge | Qt.LeftEdge:
                return CursorShape.BottomLeftCorner
            case Qt.BottomEdge | Qt.RightEdge:
                return CursorShape.BottomRightCorner
            }

            return Qt.ArrowCursor;
        }

        onPositionChanged: function (event) {
            edges = resizeMask.edgesAt(Qt.point(event.x, event.y))
        }

        onPressed: function (event) {
            // Touch presses need not have a preceding hover event.
            edges = resizeMask.edgesAt(Qt.point(event.x, event.y))
            if (edges)
                surface.resizeRequested(edges, /*overrideCursor=*/true)
        }
    }

    XdgShadow {
        id: shadow
        width: surface.presentationFrameGeometry.rect.width
        height: surface.presentationFrameGeometry.rect.height
        cornerRadius: surface.presentationFrameGeometry.radius
        anchors.centerIn: parent
        visible: surface.shadowVisible
        shadowBlur: surface.shadowBlurRadius
        shadowOffsetX: surface.shadowOffsetX
        shadowOffsetY: surface.shadowOffsetY
        shadowColor: surface.shadowColor
    }

    Border {
        visible: surface.visibleDecoration && surface.borderVisible
        parent: surface.prelaunchSplash ? surface.prelaunchSplash : surface.surfaceItem
        z: SurfaceItem.ZOrder.ContentItem + 1
        anchors.fill: parent
        radius: surface.presentationFrameGeometry.radius
        outsideColor: surface.borderColor
        borderWidth: surface.borderWidth
    }
}

// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Waylib.Server
import Treeland

Item {
    id: root

    required property SurfaceWrapper surface

    visible: surface && surface.visibleDecoration && surface.visible
    x: shadow.boundingRect.x
    y: shadow.boundingRect.y
    width: shadow.boundingRect.width
    height: shadow.boundingRect.height

    MouseArea {
        id: resizeArea

        // The decoration must not catch input rejected by the client surface,
        // including holes in an X11 input shape. Only the resize rim is ours.
        containmentMask: QtObject {
            function contains(position: point): bool {
                return WaylibHelper.getEdges(Qt.rect(0, 0, resizeArea.width, resizeArea.height), position, 10) !== 0;
            }
        }

        enabled: surface.type !== SurfaceWrapper.Type.XdgPopup
                    && surface.type !== SurfaceWrapper.Type.Layer
                    && surface.type !== SurfaceWrapper.Type.SplashScreen
        property int edges: 0

        anchors {
            fill: shadow
            margins: -10
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
            edges = WaylibHelper.getEdges(Qt.rect(0, 0, width, height), Qt.point(event.x, event.y), 10)
        }

        onPressed: function (event) {
            // Maybe missing onPositionChanged when use touchscreen
            edges = WaylibHelper.getEdges(Qt.rect(0, 0, width, height), Qt.point(event.x, event.y), 10)
            if (edges)
                surface.resizeRequested(edges)
        }
    }

    XdgShadow {
        id: shadow
        width: surface.width
        height: surface.height
        cornerRadius: surface.radius
        anchors.centerIn: parent
        visible: surface.shadowVisible
        shadowBlur: surface.shadowBlurRadius
        shadowOffsetX: surface.shadowOffsetX
        shadowOffsetY: surface.shadowOffsetY
        shadowColor: surface.shadowColor
    }

    Border {
        visible: surface.visibleDecoration && surface.borderVisible
        parent: surface.surfaceItem ? surface.surfaceItem : surface.prelaunchSplash
        z: SurfaceItem.ZOrder.ContentItem + 1
        anchors.fill: parent
        radius: surface.radius
        outsideColor: surface.borderColor
        borderWidth: surface.borderWidth
    }
}

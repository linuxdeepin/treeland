// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
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
                return Waylib.CursorShape.TopSide
            case Qt.RightEdge:
                return Waylib.CursorShape.RightSide
            case Qt.BottomEdge:
                return Waylib.CursorShape.BottomSide
            case Qt.LeftEdge:
                return Waylib.CursorShape.LeftSide
            case Qt.TopEdge | Qt.LeftEdge:
                return Waylib.CursorShape.TopLeftCorner
            case Qt.TopEdge | Qt.RightEdge:
                return Waylib.CursorShape.TopRightCorner
            case Qt.BottomEdge | Qt.LeftEdge:
                return Waylib.CursorShape.BottomLeftCorner
            case Qt.BottomEdge | Qt.RightEdge:
                return Waylib.CursorShape.BottomRightCorner
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
                surface.requestResize(edges)
        }
    }

    XdgShadow {
        id: shadow
        width: surface.width
        height: surface.height
        cornerRadius: surface.radius
        anchors.centerIn: parent
    }

    Border {
        visible: surface.visibleDecoration
        parent: surface.surfaceItem ? surface.surfaceItem : surface.prelaunchSplash
        z: SurfaceItem.ZOrder.ContentItem + 1
        anchors.fill: parent
        radius: surface.radius
    }
}

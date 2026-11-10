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

        // Respect the input region only when the X11 client has an input shape.
        containmentMask: xwaylandSurface && xwaylandSurface.hasInputShape ? inputShapeMask : null

        QtObject {
            id: inputShapeMask

            function contains(position: point): bool {
                const eventItem = root.surface.surfaceItem?.eventItem;
                if (!eventItem)
                    return false;

                // Mapping to the event item already accounts for surfaceSizeRatio.
                const local = eventItem.mapFromItem(resizeArea, position);
                return local.x >= 0 && local.y >= 0
                        && local.x < eventItem.width && local.y < eventItem.height
                        && eventItem.contains(local);
            }
        }

        enabled: surface.type !== SurfaceWrapper.Type.XdgPopup
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
            edges = WaylibHelper.getEdges(Qt.rect(0, 0, width, height), Qt.point(event.x, event.y), root.edgeSize)
        }

        onPressed: function (event) {
            // Maybe missing onPositionChanged when use touchscreen
            edges = WaylibHelper.getEdges(Qt.rect(0, 0, width, height), Qt.point(event.x, event.y), root.edgeSize)
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

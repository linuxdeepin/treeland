// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server

Item {
    id: root

    signal requestMove
    signal requestMinimize
    signal requestToggleMaximize(var max)
    signal requestClose
    signal requestResize(var edges)

    readonly property real topMargin: titlebar.height
    readonly property real bottomMargin: 0
    readonly property real leftMargin: 0
    readonly property real rightMargin: 0
    required property ToplevelSurface surface

    MouseArea {
        property int edges: 0

        anchors {
            fill: parent
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
            Helper.activatedSurface = surface
            if (edges)
                root.requestResize(edges)
        }
    }

    Rectangle {
        id: titlebar
        anchors.top: parent.top
        width: parent.width
        height: 30

        MouseArea {
            anchors.fill: parent
            Cursor.shape: pressed ? Waylib.CursorShape.Grabbing : Qt.ArrowCursor

            onPressed: {
                root.requestMove()
            }
        }

        Row {
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 8
            }

            Button {
                text: "Min"
                onClicked: root.requestMinimize()
            }
            Button {
                text: "Max"
                onClicked: {
                    const max = (text === "Max")
                    root.requestToggleMaximize(max)

                    if (max) {
                        text = "Restore"
                    } else {
                        text = "Max"
                    }
                }
            }
            Button {
                text: "Close"
                onClicked: root.requestClose()
            }
        }
    }
}

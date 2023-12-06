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

    MouseArea {
        property int edges: 0

        anchors {
            fill: parent
            margins: -5
        }

        hoverEnabled: true
        Cursor.shape: {
            switch(edges) {
            case Qt.TopEdge:
                return Cursor.TopSide
            case Qt.RightEdge:
                return Cursor.RightSide
            case Qt.BottomEdge:
                return Cursor.BottomSide
            case Qt.LeftEdge:
                return Cursor.LeftSide
            case Qt.TopEdge | Qt.LeftEdge:
                return Cursor.TopLeftCorner
            case Qt.TopEdge | Qt.RightEdge:
                return Cursor.TopRightCorner
            case Qt.BottomEdge | Qt.LeftEdge:
                return Cursor.BottomLeftCorner
            case Qt.BottomEdge | Qt.RightEdge:
                return Cursor.BottomRightCorner
            }

            return Qt.ArrowCursor;
        }

        onPositionChanged: function (event) {
            edges = WaylibHelper.getEdges(Qt.rect(0, 0, width, height), Qt.point(event.x, event.y), 10)
        }

        onPressed: {
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
            Cursor.shape: pressed ? Cursor.Grabbing : Qt.ArrowCursor

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

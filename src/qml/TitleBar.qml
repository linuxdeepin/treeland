// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Treeland

Item {
    id: root

    required property SurfaceWrapper surface
    readonly property SurfaceItem surfaceItem: surface.surfaceItem

    height: 30
    width: surfaceItem.width

    MouseArea {
        anchors.fill: parent
        Cursor.shape: pressed ? Waylib.CursorShape.Grabbing : Qt.ArrowCursor

        onPressed: {
            surface.requestMove();
        }
    }

    Rectangle {
        id: titlebar
        anchors.fill: parent
        color: surface.shellSurface.isActivated ? "white" : "gray"

        Row {
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 8
            }

            Button {
                width: titlebar.height
                text: "-"
                onClicked: surface.requestMinimize()
            }
            Button {
                width: titlebar.height
                text: "O"
                onClicked: surface.requestToggleMaximize()
            }
            Button {
                width: titlebar.height
                text: "X"
                onClicked: surface.requestClose()
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: surface.radius > 0 && !surface.noCornerRadius
        sourceComponent: RoundedClipEffect {
            anchors.fill: parent
            sourceItem: titlebar
            radius: surface.radius
            targetRect: Qt.rect(-root.x, -root.y, surfaceItem.width, surfaceItem.height)
        }
    }
}

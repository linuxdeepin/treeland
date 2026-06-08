// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Tinywl

Control {
    id: root

    required property SurfaceWrapper surface
    readonly property SurfaceItem surfaceItem: surface.surfaceItem

    height: 30
    width: surfaceItem.width

    HoverHandler {
        // block hover events to resizing mouse area, avoid cursor change
        cursorShape: Qt.ArrowCursor
    }

    //Normal mouse click
    TapHandler {
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onTapped: (eventPoint, button) => {
            if (button === Qt.RightButton) {
                surface.requestShowWindowMenu(eventPoint.position)
            } else {
                Helper.activeSurface(surface)
            }
        }
        onPressedChanged: {
            if (pressed)
                surface.requestMove()
        }

        onDoubleTapped: (_, button) => {
            if (button === Qt.LeftButton) {
                surface.requestToggleMaximize()
            }
        }
    }

    //Touch screen click
    TapHandler {
        acceptedButtons: Qt.NoButton
        acceptedDevices: PointerDevice.TouchScreen
        onDoubleTapped: surface.requestToggleMaximize()
        onLongPressed: surface.requestShowWindowMenu(point.position)
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

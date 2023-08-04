// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server

OutputPositioner {
    required property WaylandOutput waylandOutput

    output: waylandOutput
    devicePixelRatio: waylandOutput.scale
    layout: QmlHelper.layout

    OutputViewport {
        id: outputViewport

        output: waylandOutput
        devicePixelRatio: parent.devicePixelRatio
        anchors.centerIn: parent
        cursorDelegate: Item {
            required property OutputCursor cursor

            visible: cursor.visible && !cursor.isHardwareCursor

            Image {
                source: cursor.imageSource
                x: -cursor.hotspot.x
                y: -cursor.hotspot.y
                cache: false
                width: cursor.size.width
                height: cursor.size.height
                sourceClipRect: cursor.sourceRect
            }
        }

        RotationAnimation {
            id: rotationAnimator

            target: outputViewport
            duration: 200
            alwaysRunToEnd: true
        }

        Timer {
            id: setTransform

            property var scheduleTransform
            onTriggered: waylandOutput.orientation = scheduleTransform
            interval: rotationAnimator.duration / 2
        }

        function rotationOutput(orientation) {
            setTransform.scheduleTransform = orientation
            setTransform.start()

            switch(orientation) {
            case WaylandOutput.R90:
                rotationAnimator.to = -90
                break
            case WaylandOutput.R180:
                rotationAnimator.to = 180
                break
            case WaylandOutput.R270:
                rotationAnimator.to = 90
                break
            default:
                rotationAnimator.to = 0
                break
            }

            rotationAnimator.from = rotation
            rotationAnimator.start()
        }
    }

    Image {
        id: background
        source: "file:///usr/share/backgrounds/gnome/oceans.svg"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        anchors.fill: parent
    }

    Column {
        anchors {
            bottom: parent.bottom
            right: parent.right
            margins: 10
        }

        spacing: 10

        Switch {
            text: "Socket"
            onCheckedChanged: {
                masterSocket.enabled = checked
            }
            Component.onCompleted: {
                checked = masterSocket.enabled
            }
        }

        Button {
            text: "1X"
            onClicked: {
                waylandOutput.scale = 1
            }
        }

        Button {
            text: "1.5X"
            onClicked: {
                waylandOutput.scale = 1.5
            }
        }

        Button {
            text: "Normal"
            onClicked: {
                outputViewport.rotationOutput(WaylandOutput.Normal)
            }
        }

        Button {
            text: "R90"
            onClicked: {
                outputViewport.rotationOutput(WaylandOutput.R90)
            }
        }

        Button {
            text: "R270"
            onClicked: {
                outputViewport.rotationOutput(WaylandOutput.R270)
            }
        }

        Button {
            text: "Login"
            onClicked: {
                login.login();
            }
        }

        Button {
            text: "Quit"
            onClicked: {
                Qt.quit()
            }
        }
    }

    Text {
        anchors.centerIn: parent
        text: "Qt Quick in a texture"
        font.pointSize: 40
        color: "white"

        SequentialAnimation on rotation {
            id: ani
            running: true
            PauseAnimation { duration: 1500 }
            NumberAnimation { from: 0; to: 360; duration: 5000; easing.type: Easing.InOutCubic }
            loops: Animation.Infinite
        }
    }
}

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Treeland

OutputItem {
    id: rootOutputItem
    readonly property OutputViewport screenViewport: outputViewport
    property alias wallpaperVisible: wallpaper.visible
    property bool forceSoftwareCursor: false

    devicePixelRatio: output?.scale ?? devicePixelRatio

    cursorDelegate: Cursor {
        id: cursorItem

        required property QtObject outputCursor
        readonly property point position: parent.mapFromGlobal(cursor.position.x, cursor.position.y)

        cursor: outputCursor.cursor
        output: outputCursor.output.output
        x: position.x - hotSpot.x
        y: position.y - hotSpot.y
        visible: valid && outputCursor.visible
        OutputLayer.enabled: !outputCursor.output.forceSoftwareCursor
        OutputLayer.keepLayer: true
        OutputLayer.outputs: [screenViewport]
        OutputLayer.flags: OutputLayer.Cursor
        OutputLayer.cursorHotSpot: hotSpot

        themeName: Helper.cursorTheme
        sourceSize: Helper.cursorSize
    }

    OutputViewport {
        id: outputViewport

        output: rootOutputItem.output
        devicePixelRatio: parent.devicePixelRatio
        anchors.centerIn: parent

        RotationAnimation {
            id: rotationAnimator

            target: outputViewport
            duration: 200
            alwaysRunToEnd: true
        }

        Timer {
            id: setTransform

            property var scheduleTransform
            onTriggered: screenViewport.rotateOutput(scheduleTransform)
            interval: rotationAnimator.duration / 2
        }

        function rotationOutput(orientation) {
            setTransform.scheduleTransform = orientation
            setTransform.start()

            switch(orientation) {
            case WaylandOutput.R90:
                rotationAnimator.to = 90
                break
            case WaylandOutput.R180:
                rotationAnimator.to = 180
                break
            case WaylandOutput.R270:
                rotationAnimator.to = -90
                break
            default:
                rotationAnimator.to = 0
                break
            }

            rotationAnimator.from = rotation
            rotationAnimator.start()
        }
    }

    Wallpaper {
        id: wallpaper
        userId: Helper.currentUserId
        output: rootOutputItem.output
        workspace: Helper.workspace.current
        anchors.fill: parent
        asynchronous: false

        states: [
            State {
                name: "Normal"
                PropertyChanges {
                    target: wallpaper
                    scale: 1
                }
            },
            State {
                name: "Scale"
                PropertyChanges {
                    target: wallpaper
                    scale: 1.4
                }
            }
        ]

        transitions: [
            Transition {
                from: "*"
                to: "Normal"
                PropertyAnimation {
                    property: "scale"
                    duration: 1000
                    easing.type: Easing.OutExpo
                }
            },
            Transition {
                from: "*"
                to: "Scale"
                PropertyAnimation {
                    property: "scale"
                    duration: 1000
                    easing.type: Easing.OutExpo
                }
            }
        ]
    }

    function setTransform(transform) {
        screenViewport.rotationOutput(transform)
    }

    function setScale(scale) {
        screenViewport.setOutputScale(scale)
    }

    function invalidate() {
        screenViewport.invalidate()
    }
}

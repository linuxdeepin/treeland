// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Treeland

OutputItem {
    id: rootOutputItem
    readonly property OutputViewport onscreenViewport: outputViewport
    property alias wallpaperVisible: wallpaper.visible

    devicePixelRatio: output?.scale ?? devicePixelRatio

    cursorDelegate: Cursor {
        id: cursorItem

        required property QtObject outputCurosr
        readonly property point position: parent.mapFromGlobal(cursor.position.x, cursor.position.y)

        cursor: outputCurosr.cursor
        output: outputCurosr.output.output
        x: position.x - hotSpot.x
        y: position.y - hotSpot.y
        visible: valid && outputCurosr.visible
        OutputLayer.enabled: true
        OutputLayer.keepLayer: true
        OutputLayer.outputs: [onscreenViewport]
        OutputLayer.flags: OutputLayer.Cursor
        OutputLayer.cursorHotSpot: hotSpot
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
            onTriggered: onscreenViewport.rotateOutput(scheduleTransform)
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
    }

    Text {
        anchors.centerIn: parent
        text: "'Ctrl+Q' quit"
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

    function setTransform(transform) {
        onscreenViewport.rotationOutput(transform)
    }

    function setScale(scale) {
        onscreenViewport.setOutputScale(scale)
    }

    function invalidate() {
        onscreenViewport.invalidate()
    }
}

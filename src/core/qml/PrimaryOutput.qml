// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Treeland

OutputItem {
    id: rootOutputItem
    readonly property OutputViewport screenViewport: outputViewport
    property bool forceSoftwareCursor: false

    devicePixelRatio: output?.scale ?? devicePixelRatio

    cursorDelegate: Cursor {
        id: cursorItem

        required property QtObject outputCursor // outputCursor is WQuickCursor (QML_ANONYMOUS), cannot use as named type
        readonly property point rawPosition: parent.mapFromGlobal(cursor.position.x, cursor.position.y)
        readonly property real effectiveScale: rootOutputItem.devicePixelRatio || 1.0

        // Align cursor position to pixel grid to prevent blur on fractional DPR displays
        function alignToPixelGrid(value: real): real {
            return Math.round(value * effectiveScale) / effectiveScale
        }

        readonly property point position: Qt.point(
            alignToPixelGrid(rawPosition.x),
            alignToPixelGrid(rawPosition.y)
        )

        cursor: outputCursor.cursor
        output: outputCursor.output.output
        x: position.x - hotSpot.x
        y: position.y - hotSpot.y
        visible: valid && outputCursor.visible
        OutputLayer.enabled: !outputCursor.output.forceSoftwareCursor
        OutputLayer.keepLayer: true
        // qmllint disable unqualified: qmllint directive — screenViewport is an outer scope property
        OutputLayer.outputs: [screenViewport]
        OutputLayer.flags: OutputLayer.Cursor
        OutputLayer.cursorHotSpot: hotSpot

        themeName: Helper.config.cursorThemeName
        sourceSize: Qt.size(Helper.config.cursorSize, Helper.config.cursorSize)
        // qmllint enable unqualified
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

            property int scheduleTransform
            onTriggered: screenViewport.rotateOutput(scheduleTransform)
            interval: rotationAnimator.duration / 2
        }

        function rotationOutput(orientation: int): void {
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

    Item {
        clip: true
        anchors.fill: parent
        WallpaperSwitcher {
            id: wallpaper

            clip: true
            anchors.fill: parent
            output: rootOutputItem.output
            workspace: Helper.workspace.current

            readonly property int duration: 1000
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
                },
                State {
                    name: "ScaleTo1.2"
                    PropertyChanges {
                        target: wallpaper
                        scale: 1.2
                    }
                },
                State {
                    name: "ScaleWithoutAnimation"
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
                        duration: wallpaper.duration
                        easing.type: Easing.OutExpo
                    }
                },
                Transition {
                    from: "*"
                    to: "Scale"
                    PropertyAnimation {
                        property: "scale"
                        duration: wallpaper.duration
                        easing.type: Easing.OutExpo
                    }
                },
                Transition {
                    from: "*"
                    to: "ScaleTo1.2"
                    PropertyAnimation {
                        property: "scale"
                        duration: wallpaper.duration
                        easing.type: Easing.OutExpo
                    }
                },
                Transition {
                    from: "*"
                    to: "ScaleWithoutAnimation"
                    PropertyAnimation {
                        property: "scale"
                        duration: 0
                    }
                }
            ]

            Connections {
                target: Helper
                function onLaunchpadMappedChanged(output: WaylandOutput, mapped: bool) {
                    if (output !== rootOutputItem.output) {
                        return;
                    }

                    wallpaper.state = mapped ? "Scale" : "Normal"
                }

                function onShowDesktopRequested(output: WaylandOutput) {
                    if (output !== rootOutputItem.output) {
                        return;
                    }

                    wallpaper.state = "Normal"
                    wallpaper.play = true
                    wallpaper.slowDown()
                }

                function onStartLockscreened(output: WaylandOutput, showAnimation: bool) {
                    if (output !== rootOutputItem.output) {
                        return;
                    }

                    wallpaper.play = false
                    wallpaper.state = showAnimation ? "ScaleTo1.2" : "ScaleWithoutAnimation"
                }
            }
        }
    }

    function setTransform(transform: int): void {
        screenViewport.rotationOutput(transform)
    }

    function setScale(scale: real): void {
        screenViewport.setOutputScale(scale)
    }

    function invalidate(): void {
        screenViewport.invalidate()
    }
}

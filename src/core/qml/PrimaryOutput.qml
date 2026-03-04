// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

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

        required property QtObject outputCursor
        readonly property point rawPosition: parent.mapFromGlobal(cursor.position.x, cursor.position.y)
        readonly property real effectiveScale: rootOutputItem.devicePixelRatio || 1.0

        // Align cursor position to pixel grid to prevent blur on fractional DPR displays
        function alignToPixelGrid(value) {
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
        OutputLayer.outputs: [screenViewport]
        OutputLayer.flags: OutputLayer.Cursor
        OutputLayer.cursorHotSpot: hotSpot

        themeName: Helper.config.cursorThemeName
        // Scale-aware cursor size: automatically updates when output scale changes
        sourceSize: Qt.size(Helper.config.cursorSize * effectiveScale, Helper.config.cursorSize * effectiveScale)
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


    Item {
        clip: true
        anchors.fill: parent
        Item {
            id: wallpaper

            readonly property int duration: 1000
            property Wallpaper currentWallpaper: fontWallpaper
            clip: true
            anchors.fill: parent
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

            Wallpaper {
                id: backWallpaper

                anchors.fill: parent
                disableUpdate: true
                wallpaperRole: Wallpaper.Desktop
                output: rootOutputItem.output
                workspace: Helper.workspace.current
                opacity: 0
                live: false
                z: 0
                onSourceChanged: {
                    wallpaper.currentWallpaper = backWallpaper

                    backWallpaper.disableUpdate = true
                    backWallpaper.live = true
                    backWallpaper.z = 1
                    backWallpaper.opacity = 1

                    fontWallpaper.disableUpdate = false
                    fontWallpaper.z = 0
                    fontWallpaper.opacity = 0
                }
                Behavior on opacity {
                    enabled: wallpaper.state = "Normal"
                    NumberAnimation {
                        duration: 500
                        easing.type: Easing.InOutQuad
                    }
                }
            }

            Wallpaper {
                id: fontWallpaper

                anchors.fill: parent
                wallpaperRole: Wallpaper.Desktop
                output: rootOutputItem.output
                workspace: Helper.workspace.current
                z: 1
                onSourceChanged: {
                    wallpaper.currentWallpaper = fontWallpaper

                    fontWallpaper.disableUpdate = true
                    fontWallpaper.live = true
                    fontWallpaper.z = 1
                    fontWallpaper.opacity = 1

                    backWallpaper.disableUpdate = false
                    backWallpaper.live = false
                    backWallpaper.z = 0
                }

                Behavior on opacity {
                    enabled: wallpaper.state = "Normal"
                    NumberAnimation {
                        duration: 500
                        easing.type: Easing.InOutQuad
                    }
                }
            }

            Connections {
                target: Helper
                function onLaunchpadMappedChanged(output, mapped) {
                    if (output !== rootOutputItem.output) {
                        return;
                    }

                    wallpaper.state = mapped ? "Scale" : "Normal"
                }

                function onShowDesktopRequested(output) {
                    if (output !== rootOutputItem.output) {
                        return;
                    }

                    wallpaper.state = "Normal"
                    wallpaper.currentWallpaper.play = true
                    wallpaper.currentWallpaper.slowDown()
                }

                function onStartLockscreened(output, showAnimation) {
                    if (output !== rootOutputItem.output) {
                        return;
                    }

                    wallpaper.currentWallpaper.play = false
                    wallpaper.state = showAnimation ? "ScaleTo1.2" : "ScaleWithoutAnimation"
                }
            }
        }
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

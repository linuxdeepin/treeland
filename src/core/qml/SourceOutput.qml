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

        themeName: WaylandServer.config.cursorThemeName
        sourceSize: Qt.size(WaylandServer.config.cursorSize, WaylandServer.config.cursorSize)
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

        readonly property bool rotationAnimationEnabled: WaylandServer.animationConfig.outputRotation

        Timer {
            id: setTransform

            property var scheduleTransform
            onTriggered: screenViewport.rotateOutput(scheduleTransform)
            interval: rotationAnimator.duration / 2
        }

        function rotationAngle(orientation) {
            switch(orientation) {
            case WaylandOutput.R90:
                return 90
            case WaylandOutput.R180:
                return 180
            case WaylandOutput.R270:
                return -90
            default:
                return 0
            }
        }

        // Turning the switch off while rotating finishes this rotation once, so
        // the pending transform of the old request cannot overwrite a new one.
        onRotationAnimationEnabledChanged: {
            if (rotationAnimationEnabled || !rotationAnimator.running)
                return
            setTransform.stop()
            rotationAnimator.stop()
            rotation = rotationAnimator.to
            screenViewport.rotateOutput(setTransform.scheduleTransform)
        }

        function rotationOutput(orientation) {
            setTransform.scheduleTransform = orientation

            if (!rotationAnimationEnabled) {
                // Apply the visual value and the real transform together instead
                // of waiting for the half-way timer.
                setTransform.stop()
                rotationAnimator.stop()
                rotation = rotationAngle(orientation)
                screenViewport.rotateOutput(orientation)
                return
            }

            setTransform.start()
            rotationAnimator.to = rotationAngle(orientation)
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
            workspace: WaylandServer.workspace.current

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
                    enabled: WaylandServer.animationConfig.wallpaperScale
                    PropertyAnimation {
                        property: "scale"
                        duration: wallpaper.duration
                        easing.type: Easing.OutExpo
                    }
                },
                Transition {
                    from: "*"
                    to: "Scale"
                    enabled: WaylandServer.animationConfig.wallpaperScale
                    PropertyAnimation {
                        property: "scale"
                        duration: wallpaper.duration
                        easing.type: Easing.OutExpo
                    }
                },
                Transition {
                    from: "*"
                    to: "ScaleTo1.2"
                    enabled: WaylandServer.animationConfig.wallpaperScale
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
                target: WaylandServer
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
                    wallpaper.play = true
                    wallpaper.slowDown()
                }

                function onStartLockscreened(output, showAnimation) {
                    if (output !== rootOutputItem.output) {
                        return;
                    }

                    wallpaper.play = false
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

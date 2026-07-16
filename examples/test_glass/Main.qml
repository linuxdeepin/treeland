// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Waylib.Server
import GlassExample
import Treeland

Item {
    id :root

    readonly property url wallpaperSource: Helper.wallpaperSource

    property bool glassMode: true
    property bool glassBlurEnabled: true
    property real effectRadius: 34
    property real glassBezelWidth: 60
    property real glassThickness: 50
    property real glassIor: 3.0
    property real glassSpecularOpacity: 0.55
    property real glassTintOpacity: 0.08
    property real glassShadow: 0.5
    property int glassBlurMax: 36

    Shortcut {
        sequences: [StandardKey.Quit]
        context: Qt.ApplicationShortcut
        onActivated: {
            Qt.quit()
        }
    }

    OutputRenderWindow {
        id: renderWindow

        width: outputsContainer.implicitWidth
        height: outputsContainer.implicitHeight
        color: "black"

        Row {
            id: outputsContainer

            anchors.fill: parent

            DynamicCreatorComponent {
                id: outputDelegateCreator
                creator: Helper.outputCreator

                OutputItem {
                    id: rootOutputItem
                    required property WaylandOutput waylandOutput
                    readonly property OutputViewport onscreenViewport: outputViewport

                    output: waylandOutput
                    devicePixelRatio: waylandOutput.scale

                    cursorDelegate: Cursor {
                        id: cursorItem

                        required property QtObject outputCursor
                        readonly property point position: parent.mapFromGlobal(cursor.position.x, cursor.position.y)

                        cursor: outputCursor.cursor
                        output: outputCursor.output.output
                        x: position.x - hotSpot.x
                        y: position.y - hotSpot.y
                        visible: valid && outputCursor.visible
                        OutputLayer.enabled: true
                        OutputLayer.keepLayer: true
                        OutputLayer.flags: OutputLayer.Cursor
                        OutputLayer.cursorHotSpot: hotSpot
                        OutputLayer.outputs: [outputViewport]
                    }

                    OutputViewport {
                        id: outputViewport

                        output: waylandOutput
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

                    Image {
                        id: background
                        source: root.wallpaperSource
                        fillMode: Image.PreserveAspectCrop
                        anchors.fill: parent
                    }

                    Image {
                        id: iconImage
                        source: "qrc:/shaders/test-glass-icon.svg"
                        anchors.centerIn: parent
                        width: 100
                        height: 100
                        fillMode: Image.PreserveAspectFit

                        RotationAnimation on rotation {
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
            }
        }

        Item {
            id: effectPanel
            width: 500
            height: 300
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2

            MouseArea {
                anchors.fill: parent
                drag.target: effectPanel
                drag.axis: Drag.XAndYAxis
                drag.minimumX: 0
                drag.maximumX: parent.parent.width - effectPanel.width
                drag.minimumY: 0
                drag.maximumY: parent.parent.height - effectPanel.height
            }

            Loader {
                anchors.fill: parent
                sourceComponent: root.glassMode ? globalGlassComponent : globalBlurComponent
            }

            Component {
                id: globalGlassComponent
                RenderBufferBlitter {
                    id: blitter
                    anchors.fill: parent
                    smooth: true
                    GlassEffect {
                        anchors.fill: parent
                        source: blitter.content
                        radius: root.effectRadius
                        blurEnabled: root.glassBlurEnabled
                        blurMax: root.glassBlurMax
                        bezelWidth: root.glassBezelWidth
                        thickness: root.glassThickness
                        ior: root.glassIor
                        specularOpacity: root.glassSpecularOpacity
                        tintOpacity: root.glassTintOpacity
                        shadow: root.glassShadow
                        smooth: true
                    }
                }
            }

            Component {
                id: globalBlurComponent
                RenderBufferBlitter {
                    id: blitter
                    anchors.fill: parent
                    MultiEffect {
                        anchors.fill: parent
                        source: blitter.content
                        autoPaddingEnabled: false
                        blurEnabled: true
                        blur: 1.0
                        blurMax: 64
                        saturation: 0.2
                    }
                }
            }
        }

        // ── Draggable RoundBlur demo ────────────────────────────────────
        Item {
            id: roundBlurPanel
            width: 30
            height: 30
            x: (parent.width - width) / 2 + 260
            y: (parent.height - height) / 2 - 180

            MouseArea {
                anchors.fill: parent
                drag.target: roundBlurPanel
                drag.axis: Drag.XAndYAxis
                drag.minimumX: 0
                drag.maximumX: parent.parent.width - roundBlurPanel.width
                drag.minimumY: 0
                drag.maximumY: parent.parent.height - roundBlurPanel.height
            }

            RoundBlur {
                anchors.fill: parent
                radius: root.effectRadius
            }

            Text {
                anchors.left: parent.right
                text: "RoundBlur\n(drag me)"
                color: "white"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // ── Control panel ──────────────────────────────────────────────────
    Frame {
        anchors.right: parent.right
        anchors.top: parent.top
        width: 320
        height: 420
        opacity: 0.85

        Column {
            anchors.fill: parent
            spacing: 6

            Row {
                spacing: 8
                Button {
                    text: root.glassMode ? "Glass" : "Blur"
                    onClicked: root.glassMode = !root.glassMode
                }
                Button {
                    text: root.glassBlurEnabled ? "Blur: on" : "Blur: off"
                    onClicked: root.glassBlurEnabled = !root.glassBlurEnabled
                }
                Button {
                    text: "Grab Glass"
                    onClicked: {
                        effectPanel.grabToImage(function(result) {
                            const path = "/tmp/treeland-glass-grab.png"
                            if (result.saveToFile(path)) {
                                console.log("Liquid Glass grab saved", path, result.image.size)
                            } else {
                                console.warn("Liquid Glass grab failed", path)
                            }
                        })
                    }
                }
            }

            Column {
                spacing: 2
                Label { text: "radius " + Math.round(root.effectRadius); color: "white" }
                Slider { from: 0; to: 80; value: root.effectRadius; onMoved: root.effectRadius = value }
            }
            Column {
                spacing: 2
                Label { text: "bezel " + Math.round(root.glassBezelWidth); color: "white" }
                Slider { from: 2; to: 60; value: root.glassBezelWidth; onMoved: root.glassBezelWidth = value }
            }
            Column {
                spacing: 2
                Label { text: "thickness " + Math.round(root.glassThickness); color: "white" }
                Slider { from: 10; to: 200; value: root.glassThickness; onMoved: root.glassThickness = value }
            }
            Column {
                spacing: 2
                Label { text: "ior " + root.glassIor.toFixed(2); color: "white" }
                Slider { from: 1.0; to: 3.0; value: root.glassIor; onMoved: root.glassIor = value }
            }
            Column {
                spacing: 2
                Label { text: "specular " + root.glassSpecularOpacity.toFixed(2); color: "white" }
                Slider { from: 0; to: 1; value: root.glassSpecularOpacity; onMoved: root.glassSpecularOpacity = value }
            }
            Column {
                spacing: 2
                Label { text: "tint " + root.glassTintOpacity.toFixed(2); color: "white" }
                Slider { from: 0; to: 1; value: root.glassTintOpacity; onMoved: root.glassTintOpacity = value }
            }
            Column {
                spacing: 2
                Label { text: "shadow " + root.glassShadow.toFixed(2); color: "white" }
                Slider { from: 0; to: 1; value: root.glassShadow; onMoved: root.glassShadow = value }
            }
            Column {
                spacing: 2
                Label { text: "blur max " + root.glassBlurMax; color: "white" }
                Slider { from: 0; to: 96; stepSize: 1; value: root.glassBlurMax; onMoved: root.glassBlurMax = value }
            }
        }
    }
}

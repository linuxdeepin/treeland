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
    property real glassBezelWidth: 30
    property real glassThickness: 50
    property real glassDisplacementFactor: 1.0
    property real glassIor: 1.2
    property real glassDispersion: 0.012
    property real glassBrightness: 0.05
    property real glassContrast: -0.12
    property real glassSaturation: 0.4
    property real glassColorization: 0.12
    property int glassBlurMax: 36
    property real glassStrokeWidth: 1.4
    property real glassStrokeStrength: 1.5
    property real glassSpecularOpacity: 0.82
    property bool glassHighlightEnabled: true
    property bool glassRimReflectionEnabled: true
    property real glassLightAngle: -126.87
    property real glassLightPower: 3.0
    property real glassEdgeSaturation: 0.0
    property real glassReflectionOffset: 12
    property bool advancedExpanded: false

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
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        anchors.fill: parent
                        smooth: true
                    }

                    Column {
                        id: controlColumn
                        anchors {
                            bottom: parent.bottom
                            left: parent.left
                            margins: 10
                        }

                        spacing: 10

                        // ── Action buttons ──────────────────────────
                        Row {
                            spacing: 10
                            Button {
                                text: root.glassMode ? "Switch to Blur" : "Switch to Glass"
                                onClicked: root.glassMode = !root.glassMode
                            }
                            Button {
                                text: "Grab Glass"
                                onClicked: {
                                    effectPanel.grabToImage(function(result) {
                                        const path = "/tmp/treeland-liquid-glass-grab.png"
                                        if (result.saveToFile(path)) {
                                            console.log("Liquid Glass grab saved", path, result.image.size)
                                        } else {
                                            console.warn("Liquid Glass grab failed", path)
                                        }
                                    })
                                }
                            }
                            Button {
                                text: root.glassHighlightEnabled ? "Highlight: on" : "Highlight: off"
                                onClicked: root.glassHighlightEnabled = !root.glassHighlightEnabled
                            }
                            Button {
                                text: root.glassRimReflectionEnabled ? "Rim refl: on" : "Rim refl: off"
                                onClicked: root.glassRimReflectionEnabled = !root.glassRimReflectionEnabled
                            }
                            Button {
                                text: root.glassBlurEnabled ? "Blur: on" : "Blur: off"
                                onClicked: root.glassBlurEnabled = !root.glassBlurEnabled
                            }
                        }

                        // ── Common sliders (always visible, single row) ──
                        Row {
                            spacing: 20

                            Column {
                                spacing: 2
                                Label { text: "radius " + Math.round(root.effectRadius); color: "white" }
                                Slider { from: 0; to: 80; value: root.effectRadius; onMoved: root.effectRadius = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "bezel " + Math.round(root.glassBezelWidth); color: "white" }
                                Slider { from: 1; to: 80; value: root.glassBezelWidth; onMoved: root.glassBezelWidth = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "blur max " + root.glassBlurMax; color: "white" }
                                Slider { from: 0; to: 96; stepSize: 1; value: root.glassBlurMax; onMoved: root.glassBlurMax = value }
                            }
                        }

                        // ── Advanced toggle ─────────────────────────
                        Button {
                            text: root.advancedExpanded ? "▼ Advanced" : "▶ Advanced"
                            onClicked: root.advancedExpanded = !root.advancedExpanded
                        }

                        // ── Advanced sliders (collapsible, multi-column) ──
                        Row {
                            spacing: 20
                            visible: root.advancedExpanded

                            // Column A: glass material & optics
                            Column {
                                spacing: 2
                                Label { text: "thickness " + Math.round(root.glassThickness); color: "white" }
                                Slider { from: 1; to: 200; value: root.glassThickness; onMoved: root.glassThickness = value }
                                Label { text: "displace " + root.glassDisplacementFactor.toFixed(2); color: "white" }
                                Slider { from: 0; to: 4; value: root.glassDisplacementFactor; onMoved: root.glassDisplacementFactor = value }
                                Label { text: "ior " + root.glassIor.toFixed(2); color: "white" }
                                Slider { from: 1.0; to: 2.5; value: root.glassIor; onMoved: root.glassIor = value }
                                Label { text: "dispersion " + root.glassDispersion.toFixed(3); color: "white" }
                                Slider { from: 0; to: 0.1; value: root.glassDispersion; onMoved: root.glassDispersion = value }
                                Label { text: "edge sat " + root.glassEdgeSaturation.toFixed(2); color: "white" }
                                Slider { from: 0; to: 1; value: root.glassEdgeSaturation; onMoved: root.glassEdgeSaturation = value }
                            }

                            // Column B: specular & lighting
                            Column {
                                spacing: 2
                                Label { text: "stroke " + root.glassStrokeWidth.toFixed(1); color: "white" }
                                Slider { from: 0; to: 8; value: root.glassStrokeWidth; onMoved: root.glassStrokeWidth = value }
                                Label { text: "stroke str " + root.glassStrokeStrength.toFixed(2); color: "white" }
                                Slider { from: 0; to: 2; value: root.glassStrokeStrength; onMoved: root.glassStrokeStrength = value }
                                Label { text: "spec opacity " + root.glassSpecularOpacity.toFixed(2); color: "white" }
                                Slider { from: 0; to: 1; value: root.glassSpecularOpacity; onMoved: root.glassSpecularOpacity = value }
                                Label { text: "light angle " + Math.round(root.glassLightAngle) + "°"; color: "white" }
                                Slider { from: -180; to: 180; value: root.glassLightAngle; onMoved: root.glassLightAngle = value }
                                Label { text: "light pow " + root.glassLightPower.toFixed(1); color: "white" }
                                Slider { from: 1; to: 16; value: root.glassLightPower; onMoved: root.glassLightPower = value }
                                Label { text: "refl offset " + Math.round(root.glassReflectionOffset); color: "white" }
                                Slider { from: 0; to: 60; value: root.glassReflectionOffset; onMoved: root.glassReflectionOffset = value }
                            }

                            // Column C: colour grading (MultiEffect)
                            Column {
                                spacing: 2
                                Label { text: "brightness " + root.glassBrightness.toFixed(2); color: "white" }
                                Slider { from: -1; to: 1; value: root.glassBrightness; onMoved: root.glassBrightness = value }
                                Label { text: "contrast " + root.glassContrast.toFixed(2); color: "white" }
                                Slider { from: -1; to: 1; value: root.glassContrast; onMoved: root.glassContrast = value }
                                Label { text: "saturation " + root.glassSaturation.toFixed(2); color: "white" }
                                Slider { from: -1; to: 1; value: root.glassSaturation; onMoved: root.glassSaturation = value }
                                Label { text: "colorization " + root.glassColorization.toFixed(2); color: "white" }
                                Slider { from: 0; to: 1; value: root.glassColorization; onMoved: root.glassColorization = value }
                            }
                        }
                    }

                    Column {
                        anchors {
                            bottom: parent.bottom
                            right: parent.right
                            margins: 10
                        }

                        spacing: 10

                        Button {
                            text: "1X"
                            onClicked: {
                                onscreenViewport.setOutputScale(1)
                            }
                        }

                        Button {
                            text: "1.5X"
                            onClicked: {
                                onscreenViewport.setOutputScale(1.5)
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
                            text: "Quit"
                            onClicked: {
                                Qt.quit()
                            }
                        }
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
                        displacementFactor: root.glassDisplacementFactor
                        ior: root.glassIor
                        dispersion: root.glassDispersion
                        brightness: root.glassBrightness
                        contrast: root.glassContrast
                        saturation: root.glassSaturation
                        colorization: root.glassColorization
                        strokeWidth: root.glassStrokeWidth
                        strokeStrength: root.glassStrokeStrength
                        specularOpacity: root.glassSpecularOpacity
                        highlightEnabled: root.glassHighlightEnabled
                        rimReflectionEnabled: root.glassRimReflectionEnabled
                        lightAngle: root.glassLightAngle
                        lightPower: root.glassLightPower
                        edgeSaturation: root.glassEdgeSaturation
                        reflectionOffset: root.glassReflectionOffset
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
        // Uses the real RoundBlur component (Blur subclass) from the
        // GlassExample QML module, which reads glass parameters from
        // Helper.config. Drag to reposition.
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
}

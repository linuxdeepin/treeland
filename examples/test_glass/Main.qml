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
    property real effectWidth: 300
    property real effectHeight: 200
    property real effectRadius: 60
    property real glassThickness: 50
    property real glassBezelWidth: 60
    property real glassIor: 1.5
    property bool glassBlurEnabled: false
    property int glassBlurMax: 36
    property real glassBlurAmount: 0.6
    property real glassBlurMultiplier: 0.0
    property real glassSpecular: 0.0
    property real glassTint: 0.0
    property real glassRefractionMaxTan: 2.75
    property real glassContentEdgePull: 0.0
    property real glassContentRampEnd: 0.15
    property real glassProfilePower: 4.0
    property real glassInnerShadow: 0.25
    property real glassBrightness: 0.0
    property real glassContrast: 0.0
    property real glassSaturation: 0.04
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
                                text: root.glassBlurEnabled ? "Blur: on" : "Blur: off"
                                onClicked: root.glassBlurEnabled = !root.glassBlurEnabled
                            }
                        }

                        GridLayout {
                            columns: 2
                            columnSpacing: 14
                            rowSpacing: 4

                            Column {
                                spacing: 2
                                Label { text: "radius " + Math.round(root.effectRadius); color: "white" }
                                Slider { from: 4; to: 120; value: root.effectRadius; onMoved: root.effectRadius = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "blur max " + root.glassBlurMax; color: "white" }
                                Slider { from: 0; to: 128; stepSize: 1; value: root.glassBlurMax; onMoved: root.glassBlurMax = value }
                            }
                        }

                        Button {
                            text: (root.advancedExpanded ? "▼" : "▶") + " Advanced"
                            onClicked: root.advancedExpanded = !root.advancedExpanded
                        }

                        GridLayout {
                            visible: root.advancedExpanded
                            columns: 4
                            columnSpacing: 14
                            rowSpacing: 4

                            Column {
                                spacing: 2
                                Label { text: "width " + Math.round(root.effectWidth); color: "white" }
                                Slider { from: 200; to: 700; value: root.effectWidth; onMoved: root.effectWidth = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "height " + Math.round(root.effectHeight); color: "white" }
                                Slider { from: 200; to: 800; value: root.effectHeight; onMoved: root.effectHeight = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "thickness " + Math.round(root.glassThickness); color: "white" }
                                Slider { from: 10; to: 200; value: root.glassThickness; onMoved: root.glassThickness = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "bezel " + Math.round(root.glassBezelWidth); color: "white" }
                                Slider { from: 2; to: 60; value: root.glassBezelWidth; onMoved: root.glassBezelWidth = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "ior " + root.glassIor.toFixed(2); color: "white" }
                                Slider { from: 1.0; to: 3.0; stepSize: 0.05; value: root.glassIor; onMoved: root.glassIor = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "blurAmount " + root.glassBlurAmount.toFixed(2); color: "white" }
                                Slider { from: 0; to: 1; stepSize: 0.05; value: root.glassBlurAmount; onMoved: root.glassBlurAmount = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "blurMultiplier " + root.glassBlurMultiplier.toFixed(2); color: "white" }
                                Slider { from: 0; to: 4; stepSize: 0.05; value: root.glassBlurMultiplier; onMoved: root.glassBlurMultiplier = value }
                            }
                            Label {
                                text: "Optics / Profile"
                                color: "white"
                                font.bold: true
                                Layout.columnSpan: 4
                            }
                            Column {
                                spacing: 2
                                Label { text: "specular " + root.glassSpecular.toFixed(2); color: "white" }
                                Slider { from: 0; to: 1; stepSize: 0.05; value: root.glassSpecular; onMoved: root.glassSpecular = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "tint " + Math.round(root.glassTint * 100) + "%"; color: "white" }
                                Slider { from: 0; to: 0.4; stepSize: 0.01; value: root.glassTint; onMoved: root.glassTint = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "max tan " + root.glassRefractionMaxTan.toFixed(2); color: "white" }
                                Slider { from: 0.5; to: 6; stepSize: 0.05; value: root.glassRefractionMaxTan; onMoved: root.glassRefractionMaxTan = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "edge pull " + root.glassContentEdgePull.toFixed(2); color: "white" }
                                Slider { from: 0; to: 1; stepSize: 0.05; value: root.glassContentEdgePull; onMoved: root.glassContentEdgePull = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "ramp end " + root.glassContentRampEnd.toFixed(2); color: "white" }
                                Slider { from: 0.05; to: 1; stepSize: 0.05; value: root.glassContentRampEnd; onMoved: root.glassContentRampEnd = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "profilePower " + root.glassProfilePower.toFixed(2); color: "white" }
                                Slider { from: 1; to: 10; stepSize: 0.1; value: root.glassProfilePower; onMoved: root.glassProfilePower = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "innerShadow " + root.glassInnerShadow.toFixed(2); color: "white" }
                                Slider { from: 0; to: 1; stepSize: 0.05; value: root.glassInnerShadow; onMoved: root.glassInnerShadow = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "brightness " + root.glassBrightness.toFixed(2); color: "white" }
                                Slider { from: -1; to: 1; stepSize: 0.05; value: root.glassBrightness; onMoved: root.glassBrightness = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "contrast " + root.glassContrast.toFixed(2); color: "white" }
                                Slider { from: -1; to: 1; stepSize: 0.05; value: root.glassContrast; onMoved: root.glassContrast = value }
                            }
                            Column {
                                spacing: 2
                                Label { text: "saturation " + root.glassSaturation.toFixed(2); color: "white" }
                                Slider { from: -1; to: 1; stepSize: 0.05; value: root.glassSaturation; onMoved: root.glassSaturation = value }
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
            width: root.effectWidth
            height: root.effectHeight
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
                        blurAmount: root.glassBlurAmount
                        blurMultiplier: root.glassBlurMultiplier
                        bezelWidth: root.glassBezelWidth
                        thickness: root.glassThickness
                        ior: root.glassIor
                        specular: root.glassSpecular
                        tint: root.glassTint
                        refractionMaxTan: root.glassRefractionMaxTan
                        contentEdgePull: root.glassContentEdgePull
                        contentRampEnd: root.glassContentRampEnd
                        profilePower: root.glassProfilePower
                        innerShadow: root.glassInnerShadow
                        smooth: true
                        brightness: root.glassBrightness
                        contrast: root.glassContrast
                        saturation: root.glassSaturation
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

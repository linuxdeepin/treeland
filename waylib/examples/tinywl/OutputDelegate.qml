// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Tinywl

OutputItem {
    id: rootOutputItem
    required property WaylandOutput waylandOutput
    property OutputViewport onscreenViewport: outputViewport
    property Cursor lastActiveCursorItem

    output: waylandOutput
    devicePixelRatio: waylandOutput.scale

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

        function updateActiveCursor() {
            if (cursorItems.size === 1) {
                lastActiveCursorItem = this;
                return;
            }

            const pos = onscreenViewport.mapToOutput(this, Qt.point(0, 0));
            if (pos.x >= 0 && pos.x < onscreenViewport.width
                    && pos.y >= 0 && pos.y < onscreenViewport.height) {
                lastActiveCursorItem = this;
            }
        }

        onXChanged: updateActiveCursor()
        onYChanged: updateActiveCursor()

        SurfaceItem {
            id: dragIcon
            parent: cursorItem.parent
            z: cursorItem.z - 1
            flags: SurfaceItem.DontCacheLastBuffer
            surface: cursorItem.cursor.requestedDragSurface
            x: cursorItem.position.x
            y: cursorItem.position.y
        }
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
        source: "file:///usr/share/wallpapers/deepin/desktop.jpg"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        anchors.fill: parent
    }

    Component {
        id: outputScaleEffect

        OutputViewport {
            readonly property OutputItem outputItem: waylandOutput.OutputItem.item

            id: viewport
            input: this
            output: waylandOutput
            devicePixelRatio: outputViewport.devicePixelRatio
            anchors.fill: outputViewport
            rotation: outputViewport.rotation

            TextureProxy {
                sourceItem: outputViewport
                anchors.fill: parent
            }

            Item {
                width: outputItem.width
                height: outputItem.height
                anchors.centerIn: parent
                rotation: -outputViewport.rotation

                Item {
                    y: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width / 2
                    height: parent.height / 3
                    clip: true

                    Item {
                        id: centerItem
                        width: 1
                        height: 1
                        anchors.centerIn: parent
                        rotation: outputViewport.rotation

                        TextureProxy {
                            id: magnifyingLens

                            sourceItem: outputViewport
                            smooth: false
                            scale: 10
                            transformOrigin: Item.TopLeft
                            width: viewport.width
                            height: viewport.height

                            function updatePosition() {
                                const pos = outputItem.lastActiveCursorItem.mapToItem(outputViewport, Qt.point(0, 0))
                                x = - pos.x * scale
                                y = - pos.y * scale
                            }

                            Connections {
                                target: outputItem.lastActiveCursorItem

                                function onXChanged() {
                                    magnifyingLens.updatePosition()
                                }

                                function onYChanged() {
                                    magnifyingLens.updatePosition()
                                }
                            }

                            Component.onCompleted: updatePosition()
                        }
                    }
                }
            }
        }
    }

    Text {
        anchors.bottom: parent.bottom
        text: {
            if (!lastActiveCursorItem)
                return "";
            let layer = lastActiveCursorItem.OutputLayer;
            return layer.inOutputsByHardware.includes(onscreenViewport)
                    ? "Hardware Cursor"
                    : "Software Cursor";
        }
        color: "red"
        font.pointSize: 20
    }

    Column {
        anchors {
            bottom: parent.bottom
            right: parent.right
            margins: 10
        }

        spacing: 10

        Switch {
            property OutputViewport outputViewportEffect

            text: "Magnifying Lens"
            onCheckedChanged: {
                if (checked) {
                    outputViewport.cacheBuffer = true
                    outputViewport.offscreen = true
                    outputViewportEffect = outputScaleEffect.createObject(outputViewport.parent)
                    onscreenViewport = outputViewportEffect
                } else {
                    outputViewportEffect.invalidate()
                    outputViewportEffect.destroy()
                    outputViewport.offscreen = false
                    outputViewport.cacheBuffer = false
                    onscreenViewport = outputViewport
                }
            }
        }

        Switch {
            text: "Socket"
            checked: true
            onCheckedChanged: {
                Helper.setSocketEnabled(checked)
            }
        }

        Switch {
            text: "Hardware Cursor"
            checkable: false
            checked: !onscreenViewport.disableHardwareLayers
            onClicked: {
                onscreenViewport.disableHardwareLayers = !onscreenViewport.disableHardwareLayers
            }
        }

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

    Component.onDestruction: onscreenViewport.invalidate()
}

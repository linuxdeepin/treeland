// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server

OutputItem {
    id: rootOutputItem
    required property WaylandOutput waylandOutput
    property OutputViewport onscreenViewport: outputViewport
    property Cursor waylandCursor

    output: waylandOutput
    devicePixelRatio: waylandOutput.scale
    layout: QmlHelper.layout
    cursorDelegate: Item {
        required property OutputCursor cursor

        visible: cursor.visible && !cursor.isHardwareCursor

        Image {
            id: cursorImage
            source: cursor.imageSource
            x: -cursor.hotspot.x
            y: -cursor.hotspot.y
            cache: false
            width: cursor.size.width
            height: cursor.size.height
            sourceClipRect: cursor.sourceRect
        }

        SurfaceItem {
            id: dragIcon
            z: cursorImage.z - 1
            flags: SurfaceItem.DontCacheLastBuffer
            visible: waylandCursor.dragSurface !== null
            surface: waylandCursor.dragSurface
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
        source:  personalizationManager.currentWallpaper
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        anchors.fill: parent
    }

    Component {
        id: outputScaleEffect

        OutputViewport {
            readonly property OutputItem outputItem: waylandOutput.OutputItem.item

            root: true
            output: waylandOutput
            devicePixelRatio: outputViewport.devicePixelRatio

            TextureProxy {
                sourceItem: outputViewport
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
            text: "2X"
            onClicked: {
                onscreenViewport.setOutputScale(2)
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
            visible: false
            text: "Quit"
            onClicked: {
                Qt.quit()
            }
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

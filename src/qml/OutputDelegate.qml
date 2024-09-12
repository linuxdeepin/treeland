// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import TreeLand
import TreeLand.Protocols

OutputItem {
    id: rootOutputItem
    required property WaylandOutput waylandOutput
    property OutputViewport onscreenViewport: outputViewport
    property var attachVirtualOutput: VirtualOutputV1.Attach(outputViewport, textureProxp)
    property var attachViewport: attachVirtualOutput.outputViewport
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

        themeName: PersonalizationV1.cursorTheme
        sourceSize: PersonalizationV1.cursorSize

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

    Item {
        id: copyItem
        anchors.fill: parent

        TextureProxy {
            // TODO: Screen parameters in copy mode also support individual settings,
            // such as resolution, rotation, zoom system refresh rate, etc.
            id: textureProxp
            sourceItem: attachViewport
            anchors.fill: parent
            rotation: attachViewport.rotation
            width: attachViewport.implicitWidth
            height: attachViewport.implicitHeight
            smooth: true
            transformOrigin: Item.Center
            scale: attachViewport.output.scale
        }
    }

    OutputViewport {
        id: outputViewport

        input: (waylandOutput && (waylandOutput !== attachViewport.output)) ? copyItem : null

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

    Wallpaper {
        id: background
        anchors.fill: parent
        outputItem: rootOutputItem

        Component.onCompleted: {
            let name = waylandOutput.name;
            var source = PersonalizationV1.background(name) + "?" + new Date().getTime()

            background.isAnimated = PersonalizationV1.isAnimagedImage(source)
            background.source = source
            WallpaperColorV1.updateWallpaperColor(name, PersonalizationV1.backgroundIsDark(name));
        }

        Connections {
            target: PersonalizationV1
            function onBackgroundChanged(outputName, isdark) {
                let name = waylandOutput.name;
                if (outputName === name) {
                    var source = PersonalizationV1.background(outputName) + "?" + new Date().getTime()

                    background.isAnimated = PersonalizationV1.isAnimagedImage(source)
                    background.source = source
                    WallpaperColorV1.updateWallpaperColor(outputName, isdark);
                }
            }
        }
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

    function setTransform(transform) {
        onscreenViewport.rotationOutput(transform)
    }

    function setScale(scale) {
        onscreenViewport.setOutputScale(scale)
    }

    Component.onDestruction: onscreenViewport.invalidate()
}

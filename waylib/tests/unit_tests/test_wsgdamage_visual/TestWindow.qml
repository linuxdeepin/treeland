// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

Item {
    id: root
    width: 800
    height: 480

    OutputRenderWindow {
        id: renderWindow
        objectName: "renderWindow"
        width: 800
        height: 480

        DynamicCreatorComponent {
            creator: Helper.outputCreator

            OutputItem {
                required property WaylandOutput waylandOutput
                output: waylandOutput
                layout: Helper.outputLayout
                devicePixelRatio: waylandOutput.scale

                cursorDelegate: Cursor {
                    id: cursorItem
                    objectName: "outputCursor"
                    required property QtObject outputCursor
                    readonly property point position: parent.mapFromGlobal(cursor.position.x, cursor.position.y)

                    cursor: outputCursor.cursor
                    output: outputCursor.output.output
                    x: position.x - hotSpot.x
                    y: position.y - hotSpot.y
                    // Hardware-cursor tests hide this item (the cursor lives on
                    // another plane). Binding must include the flag so C++
                    // setVisible(false) is not overwritten by outputCursor.visible.
                    property bool simulateHardwareCursor: false
                    visible: valid && outputCursor.visible && !simulateHardwareCursor
                    // Tests switch OutputLayer / hardware at runtime. Default
                    // matches treeland: independent cursor layer.
                    OutputLayer.enabled: true
                    OutputLayer.keepLayer: true
                    OutputLayer.outputs: [outputViewport]
                    OutputLayer.flags: OutputLayer.Cursor
                    OutputLayer.cursorHotSpot: hotSpot
                }

                OutputViewport {
                    id: outputViewport
                    objectName: "outputViewport"
                    output: waylandOutput
                    devicePixelRatio: parent.devicePixelRatio
                    anchors.fill: parent
                }

                DamageScene {
                    id: scene
                    objectName: "scene"
                    anchors.fill: parent
                }

                DynamicCreatorComponent {
                    creator: Helper.xdgShellCreator

                    XdgToplevelSurfaceItem {
                        objectName: "clientSurface"
                        required property WaylandXdgSurface waylandSurface
                        shellSurface: waylandSurface
                        x: 20
                        y: 360
                    }
                }
            }
        }
    }
}

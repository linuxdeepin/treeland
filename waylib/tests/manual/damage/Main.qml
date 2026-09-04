// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import DamagePlayground

Item {
    id: root

    Shortcut {
        sequences: [StandardKey.Quit]
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: {
            // 占位：真实损伤演示由 DemoScene 控制
        }
    }

    OutputRenderWindow {
        id: renderWindow
        width: outputsContainer.implicitWidth
        height: outputsContainer.implicitHeight

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
                    }

                    // 真实 Waylib 损伤演示：布局与 damagegraph-demo 1:1，渲染经 WSGBatchRenderer
                    DamageView {
                        anchors.fill: parent
                    }
                }
            }
        }
    }
}

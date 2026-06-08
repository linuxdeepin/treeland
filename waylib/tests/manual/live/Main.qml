// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Waylib.Server
import Live

Item {
    id :root

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
                    layout: outputLayout
                    cursorDelegate: Cursor {
                        id: cursorItem

                        readonly property point position: parent.mapFromGlobal(cursor.position.x, cursor.position.y)

                        x: position.x - hotSpot.x
                        y: position.y - hotSpot.y
                        visible: valid && cursor.visible
                        OutputLayer.enabled: true
                        OutputLayer.keepLayer: true
                        OutputLayer.outputs: [outputViewport]
                    }

                    OutputViewport {
                        id: outputViewport
                        input: contents
                        output: waylandOutput
                        anchors.centerIn: parent
                    }

                    Item {
                        id: contents
                        anchors.fill: parent
                        Rectangle {
                            anchors.fill: parent
                            color: "black"
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "'Ctrl+Q' quit"
                            font.pointSize: 40
                            color: "white"
                        }
                        DynamicCreatorComponent {
                            id: toplevelComponent
                            creator: Helper.xdgShellCreator
                            chooserRole: "type"
                            chooserRoleValue: "toplevel"
                            XdgSurfaceItem {
                                id: toplevelSurfaceItem
                                required property WaylandXdgSurface waylandSurface
                                shellSurface: waylandSurface
                                Button {
                                    id: button
                                    text: "Freeze"
                                    onClicked: {
                                        const freeze = (text === "Freeze")
                                        toplevelSurfaceItem.contentItem.live = !freeze
                                        if (freeze) {
                                            text = "Unfreeze"
                                        } else {
                                            text = "Freeze"
                                        }
                                    }
                                }
                                topPadding: button.height
                                anchors.centerIn: parent
                            }
                        }
                    }
                }
            }
        }
    }
}

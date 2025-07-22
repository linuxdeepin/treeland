// Copyright (C) 2024 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Waylib.Server

Item {
    id :root

    WaylandServer {
        id: server

        WaylandBackend {
            id: backend

            onOutputAdded: function(output) {
                if (!backend.hasDrm)
                    output.forceSoftwareCursor = true // Test
                outputManager.add({waylandOutput: output})
            }
            onOutputRemoved: function(output) {
                output.OutputItem.item.invalidate()
                outputManager.removeIf(function(prop) {
                    return prop.waylandOutput === output
                })
            }
            onInputAdded: function(inputDevice) {
                seat0.addDevice(inputDevice)
            }
            onInputRemoved: function(inputDevice) {
                seat0.removeDevice(inputDevice)
            }
        }

        WaylandCompositor {
            id: compositor

            backend: backend
        }

        XdgShell {
            id: shell

            onSurfaceAdded: function(surface) {
                xdgSurfaceManager.add({waylandSurface: surface})
            }
            onSurfaceRemoved: function(surface) {
                xdgSurfaceManager.removeIf(function(prop) {
                    return prop.waylandSurface === surface
                })
            }
        }

        Seat {
            id: seat0
            name: "seat0"
            cursor: Cursor {
                id: cursor

                layout: outputLayout
            }
        }

        WaylandSocket {
            freezeClientWhenDisable: false

            Component.onCompleted: {
                console.info("Listing on:", socketFile)
                helper.startDemo(socketFile)
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Quit]
        context: Qt.ApplicationShortcut
        onActivated: {
            Qt.quit()
        }
    }

    DynamicCreator {
        id: outputManager
    }

    DynamicCreator {
        id: xdgSurfaceManager
    }

    OutputLayout {
        id: outputLayout
    }

    OutputRenderWindow {
        id: renderWindow

        compositor: compositor
        width: outputsContainer.implicitWidth
        height: outputsContainer.implicitHeight

        Row {
            id: outputsContainer

            anchors.fill: parent

            DynamicCreatorComponent {
                id: outputDelegateCreator
                creator: outputManager

                OutputItem {
                    id: rootOutputItem
                    required property WaylandOutput waylandOutput

                    output: waylandOutput
                    devicePixelRatio: waylandOutput.scale
                    layout: outputLayout
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
                    }

                    Image {
                        id: background
                        source: "file:///usr/share/wallpapers/deepin/desktop.jpg"
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        anchors.fill: parent
                    }

                    DynamicCreatorComponent {
                        creator: xdgSurfaceManager

                        XdgSurfaceItem {
                            required property WaylandXdgSurface waylandSurface
                            shellSurface: waylandSurface

                            delegate: Rectangle {
                                required property SurfaceItem surface

                                anchors.fill: parent
                                color: "red"

                                SurfaceItemContent {
                                    surface: parent.surface.shellSurface.surface
                                    anchors.fill: parent
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

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
                    cursorDelegate: Item {
                        required property OutputCursor cursor

                        visible: cursor.visible && !cursor.isHardwareCursor
                        width: cursor.size.width
                        height: cursor.size.height

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
                            surface: waylandSurface

                            delegate: Rectangle {
                                required property SurfaceItem surface

                                anchors.fill: parent
                                color: "red"

                                SurfaceItemContent {
                                    surface: parent.surface.surface.surface
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

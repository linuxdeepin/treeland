// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Tinywl

Item {
    id :root

    WaylandServer {
        id: server

        WaylandBackend {
            id: backend

            onOutputAdded: function(output) {
                output.forceSoftwareCursor = true // Test

                if (outputModel.count > 0)
                    output.scale = 2

                outputModel.append({waylandOutput: output})
            }
            onOutputRemoved: function(output) {

            }
            onInputAdded: function(inputDevice) {
                seat0.addDevice(inputDevice)
            }
        }

        WaylandCompositor {
            id: compositor

            backend: backend
        }

        XdgShell {
            id: shell

            onSurfaceRequestMap: function(surface) {
                renderWindow.surfaceModel.append({waylandSurface: surface})
            }
        }

        Seat {
            id: seat0
            name: "seat0"
            cursor: Cursor {
                id: cursor1

                layout: layout
                delegate: Image {
                    sourceSize: cursor1.size
                    source: cursor1.cursorUrl
                }
            }
        }
    }

    MoveResizeHelper {
        id: moveResizeHelper
        seat: seat0.seat
    }

    OutputLayout {
        id: layout
    }

    OutputRenderWindow {
        id: renderWindow

        property ListModel surfaceModel

        compositor: compositor
        layout: layout

        Row {
            Repeater {
                model: ListModel {
                    id: outputModel
                }

                OutputViewport {
                    id: outputWindow

                    required property WaylandOutput waylandOutput

                    output: waylandOutput

                    Image {
                        id: background
                        sourceSize: Qt.size(parent.width, parent.height)
                        source: "file:///usr/share/backgrounds/deepin/desktop.jpg"
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        anchors.fill: parent
                    }

                    Repeater {
                        id: re
                        anchors.fill: parent
                        clip: false

                        model: ListModel {
                            Component.onCompleted: {
                                renderWindow.surfaceModel = this
                            }
                        }

                        SurfaceItem {
                            id: surfaceItem
                            required property WaylandSurface waylandSurface

                            surface: waylandSurface

                            Connections {
                                target: surfaceItem.waylandSurface

                                function onRequestMove(seat, serial) {
                                    if (seat === moveResizeHelper.seat)
                                        moveResizeHelper.startMove(surfaceItem.waylandSurface, surfaceItem, serial)
                                }
                            }

                            Component.onCompleted: {
                                x = (parent.width - width) / 2
                                y = (parent.height - height) / 2
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
                                outputWindow.output.scale = 1
                            }
                        }

                        Button {
                            text: "1.5X"
                            onClicked: {
                                outputWindow.output.scale = 1.5
                            }
                        }

                        Button {
                            text: "Normal"
                            onClicked: {
                                outputWindow.output.orientation = Output.Normal
                            }
                        }

                        Button {
                            text: "R90"
                            onClicked: {
                                outputWindow.output.orientation = Output.R90
                            }
                        }

                        Button {
                            text: "R270"
                            onClicked: {
                                outputWindow.output.orientation = Output.R270
                            }
                        }

                        Button {
                            text: "Quit"
                            onClicked: {
                                Qt.quit()
                            }
                        }
                    }
                }
            }
        }
    }
}

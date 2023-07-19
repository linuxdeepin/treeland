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

                outputModel.append({waylandOutput: output, outputLayout: layout})
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

            onSurfaceAdded: function(surface) {
                renderWindow.surfaceModel.append({waylandSurface: surface, outputLayout: layout})
            }
            onSurfaceRemoved: function(surface) {
                for (var i = 0; i < renderWindow.surfaceModel.count; ++i) {
                    if (renderWindow.surfaceModel.get(i).waylandSurface === surface)
                        renderWindow.surfaceModel.remove(i)
                }
            }

            onRequestMove: function(surface, seat, serial) {
                eventFilter.startMove(surface, seat, serial)
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

            eventFilter: eventFilter
        }
    }

    EventFilter {
        id: eventFilter
    }

    OutputLayout {
        id: layout
    }

    OutputRenderWindow {
        id: renderWindow

        property ListModel surfaceModel

        compositor: compositor
        width: outputRowLayout.implicitWidth + outputRowLayout.x
        height: outputRowLayout.implicitHeight + outputRowLayout.y

        Row {
            id: outputRowLayout

            Repeater {
                model: ListModel {
                    id: outputModel
                }

                OutputPositioner {
                    required property WaylandOutput waylandOutput
                    required property OutputLayout outputLayout

                    output: waylandOutput
                    devicePixelRatio: waylandOutput.scale
                    layout: outputLayout

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
                            onTriggered: waylandOutput.orientation = scheduleTransform
                            interval: rotationAnimator.duration / 2
                        }

                        function rotationOutput(orientation) {
                            setTransform.scheduleTransform = orientation
                            setTransform.start()

                            switch(orientation) {
                            case WaylandOutput.R90:
                                rotationAnimator.to = -90
                                break
                            case WaylandOutput.R180:
                                rotationAnimator.to = 180
                                break
                            case WaylandOutput.R270:
                                rotationAnimator.to = 90
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
                        source: "file:///usr/share/backgrounds/deepin/desktop.jpg"
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        anchors.fill: parent
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
                                waylandOutput.scale = 1
                            }
                        }

                        Button {
                            text: "1.5X"
                            onClicked: {
                                waylandOutput.scale = 1.5
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
                        text: "Qt Quick in a texture"
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
                }
            }
        }

        Repeater {
            anchors.fill: parent
            clip: false

            model: ListModel {
                Component.onCompleted: {
                    if (!renderWindow.surfaceModel)
                        renderWindow.surfaceModel = this
                }
            }

            FocusScope {
                required property WaylandSurface waylandSurface
                required property OutputLayout outputLayout

                visible: waylandSurface && waylandSurface.mapped
                width: surfaceItem.width
                height: surfaceItem.height
                x: waylandSurface.position.x
                y: waylandSurface.position.y
                z: focus ? OutputLayout.ActiveToplevelSurface : OutputLayout.ToplevelSurface

                onFocusChanged: {
                    if (typeof waylandSurface.setActivate === "function")
                        waylandSurface.setActivate(focus)
                    if (focus)
                        surfaceItem.forceActiveFocus()
                    else
                        surfaceItem.focus = false
                }

                OutputLayoutItem {
                    anchors.fill: parent
                    layout: outputLayout

                    onEnterOutput: function(output) {
                        waylandSurface.enterOutput(output);
                    }
                    onLeaveOutput: function(output) {
                        waylandSurface.leaveOutput(output);
                    }
                }

                SurfaceItem {
                    id: surfaceItem

                    surface: waylandSurface
                }

                Component.onCompleted: {
                    waylandSurface.shell = this
                    if (waylandSurface.parentSurface)
                        parent = waylandSurface.parentSurface.shell
                }
            }
        }
    }
}

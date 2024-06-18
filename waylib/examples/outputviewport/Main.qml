// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
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

        Seat {
            id: seat0
            name: "seat0"
            cursor: Cursor {
                id: cursor

                layout: outputLayout
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

    OutputLayout {
        id: outputLayout
    }

    OutputRenderWindow {
        id: renderWindow

        compositor: compositor
        width: outputsContainer.implicitWidth
        height: outputsContainer.implicitHeight

        onOutputViewportInitialized: function (viewport) {
            // Trigger QWOutput::frame signal in order to ensure WOutputHelper::renderable
            // property is true, OutputRenderWindow when will render this output in next frame.
            Helper.enableOutput(viewport.output)
        }

        Row {
            id: outputsContainer

            anchors.fill: parent

            DynamicCreatorComponent {
                id: outputDelegateCreator
                creator: outputManager

                OutputItem {
                    id: rootOutputItem
                    required property WaylandOutput waylandOutput
                    readonly property OutputViewport onscreenViewport: outputViewport

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

                        input: outputViewportInputSelector.currentIndex === 0 ? null : contents
                        scale: input ? 1.1 : 1
                        live: input ? false : true
                        output: waylandOutput
                        devicePixelRatio: parent.devicePixelRatio
                        anchors {
                            centerIn: parent
                            horizontalCenterOffset: input ? 50 : 0
                        }

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

                    Item {
                        id: contents
                        anchors.fill: parent

                        Image {
                            id: background
                            source: "file:///usr/share/wallpapers/deepin/desktop.jpg"
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

                            ComboBox {
                                id: outputViewportInputSelector
                                model: ["Root Item(live)", "Non Root Item(not live)"]
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
            }
        }
    }
}

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Waylib.Server
import OutputViewport

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

                        required property QtObject outputCurosr
                        readonly property point position: parent.mapFromGlobal(cursor.position.x, cursor.position.y)

                        cursor: outputCurosr.cursor
                        output: outputCurosr.output.output
                        x: position.x - hotSpot.x
                        y: position.y - hotSpot.y
                        visible: valid && outputCurosr.visible
                        OutputLayer.enabled: true
                        OutputLayer.keepLayer: true
                        OutputLayer.flags: OutputLayer.Cursor
                        OutputLayer.cursorHotSpot: hotSpot
                        OutputLayer.outputs: [outputViewport]
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

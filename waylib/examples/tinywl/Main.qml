// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
                let type = surface.isPopup ? "popup" : "toplevel"
                xdgSurfaceManager.add({type: type, waylandSurface: surface, outputLayout: layout})
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
                id: cursor1

                layout: layout
            }

            eventFilter: Helper
            keyboardFocus: Helper.getFocusSurfaceFrom(renderWindow.activeFocusItem)
        }

        WaylandSocket {
            id: masterSocket

            freezeClientWhenDisable: false

            Component.onCompleted: {
                console.info("Listing on:", socketFile)
                Helper.startDemoClient(socketFile)
            }
        }
    }

    DynamicCreator {
        id: xdgSurfaceManager

        function printStructureObject(obj) {
            var json = ""
            for (var prop in obj){
                if (!obj.hasOwnProperty(prop)){
                    continue;
                }
                json += `    ${prop}: ${obj[prop]},\n`
            }

            return '{\n' + json + '}'
        }

        onObjectAdded: function(delegate, obj, properties) {
            console.info(`New Xdg surface item ${obj} from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }
        onObjectRemoved: function(delegate, obj, properties) {
            console.info(`Xdg surface item ${obj} Removed, it's create from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }
    }

    OutputLayout {
        id: layout
    }

    OutputRenderWindow {
        id: renderWindow

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
                        cursorDelegate: Item {
                            required property OutputCursor cursor

                            visible: cursor.visible && !cursor.isHardwareCursor

                            Image {
                                source: cursor.imageSource
                                x: -cursor.hotspot.x
                                y: -cursor.hotspot.y
                                cache: false
                                width: cursor.size.width
                                height: cursor.size.height
                                sourceClipRect: cursor.sourceRect
                            }
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

                        Switch {
                            text: "Socket"
                            onCheckedChanged: {
                                masterSocket.enabled = checked
                            }
                            Component.onCompleted: {
                                checked = masterSocket.enabled
                            }
                        }

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

        ColumnLayout {
            anchors.fill: parent

            TabBar {
                id: layoutChooser

                Layout.fillWidth: true

                TabButton {
                    text: qsTr("Stack Layout")
                }
                TabButton {
                    text: qsTr("Tiled Layout")
                }
            }

            Loader {
                id: workspaceLoader

                Layout.fillWidth: true
                Layout.fillHeight: true

                active: true
                source: layoutChooser.currentIndex === 0 ? "XdgStackWorkspace.qml" : "XdgTiledWorkspace.qml"

                onLoaded: {
                    item.creator = xdgSurfaceManager
                }
            }
        }
    }
}

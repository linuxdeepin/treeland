// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Waylib.Server
import TreeLand
import TreeLand.Greeter

Item {
    id :root

    WaylandServer {
        id: server

        ShortcutManager {
            helper: TreeLandHelper
        }

        TreeLandForeignToplevelManagerV1 {
            id: treelandForeignToplevelManager
        }

        SocketManager {
            onNewSocket: {
                socketProxy.newSocket(username, fd)
            }
        }

        WaylandSocketProxy {
            id: socketProxy
            Component.onCompleted: {
                TreeLand.socketProxy = socketProxy
            }
        }

        ExtForeignToplevelList {
            id: extForeignToplevelList
        }

        WaylandBackend {
            id: backend

            onOutputAdded: function(output) {
                output.forceSoftwareCursor = true // Test

                if (QmlHelper.outputManager.count > 0)
                    output.scale = 2

                TreeLandHelper.allowNonDrmOutputAutoChangeMode(output)
                QmlHelper.outputManager.add({waylandOutput: output})
            }
            onOutputRemoved: function(output) {
                QmlHelper.outputManager.removeIf(function(prop) {
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
                let type = surface.isPopup ? "popup" : "toplevel"
                QmlHelper.xdgSurfaceManager.add({type: type, waylandSurface: surface})

                if (!surface.isPopup) {
                    extForeignToplevelList.add(surface)
                    treelandForeignToplevelManager.add(surface)
                }
            }
            onSurfaceRemoved: function(surface) {
                QmlHelper.xdgSurfaceManager.removeIf(function(prop) {
                    return prop.waylandSurface === surface
                })

                if (!surface.isPopup) {
                    extForeignToplevelList.remove(surface)
                    treelandForeignToplevelManager.remove(surface)
                }
            }
        }

        LayerShell {
            id: layerShell

            onSurfaceAdded: function(surface) {
                QmlHelper.layerSurfaceManager.add({waylandSurface: surface})
            }
            onSurfaceRemoved: function(surface) {
                QmlHelper.layerSurfaceManager.removeIf(function(prop) {
                    return prop.waylandSurface === surface
                })
            }
        }

        Seat {
            id: seat0
            name: "seat0"
            cursor: Cursor {
                id: cursor1

                layout: QmlHelper.layout
            }

            eventFilter: TreeLandHelper
            keyboardFocus: TreeLandHelper.getFocusSurfaceFrom(renderWindow.activeFocusItem)
        }

        CursorShapeManager { }

        WaylandSocket {
            id: masterSocket

            freezeClientWhenDisable: false

            Component.onCompleted: {
                console.info("Listing on:", socketFile)
                TreeLandHelper.socketFile = socketFile
            }
        }

        // TODO: add attached property for XdgSurface
        XdgDecorationManager {
            id: decorationManager
        }

        InputMethodManagerV2 {
            id: inputMethodManagerV2
        }

        TextInputManagerV1 {
            id: textInputManagerV1
        }

        TextInputManagerV3 {
            id: textInputManagerV3
        }

        VirtualKeyboardManagerV1 {
            id: virtualKeyboardManagerV1
        }

        XWayland {
            id: xwayland
            compositor: compositor.compositor
            seat: seat0.seat
            lazy: false

            onReady: masterSocket.addClient(client())

            onSurfaceAdded: function(surface) {
                QmlHelper.xwaylandSurfaceManager.add({waylandSurface: surface})
            }
            onSurfaceRemoved: function(surface) {
                QmlHelper.xwaylandSurfaceManager.removeIf(function(prop) {
                    return prop.waylandSurface === surface
                })
            }
        }
    }

    InputMethodHelper {
        id: inputMethodHelperSeat0
        seat: seat0
        textInputManagerV1: textInputManagerV1
        textInputManagerV3: textInputManagerV3
        inputMethodManagerV2: inputMethodManagerV2
        virtualKeyboardManagerV1: virtualKeyboardManagerV1
        activeFocusItem: renderWindow.activeFocusItem.parent
        onInputPopupSurfaceV2Added: function (surface) {
            QmlHelper.inputPopupSurfaceManager.add({ popupSurface: surface, inputMethodHelper: inputMethodHelperSeat0 })
        }
        onInputPopupSurfaceV2Removed: function (surface) {
            QmlHelper.inputPopupSurfaceManager.removeIf(function (prop) {
                return prop.waylandSurface === surface
            })
        }
    }

    OutputRenderWindow {
        id: renderWindow

        compositor: compositor
        width: outputRowLayout.implicitWidth + outputRowLayout.x
        height: outputRowLayout.implicitHeight + outputRowLayout.y

        EventJunkman {
            anchors.fill: parent
        }

        Row {
            id: outputRowLayout

            DynamicCreatorComponent {
                creator: QmlHelper.outputManager

                OutputDelegate {
                    property real topMargin: topbar.height
                }
            }
        }

        MessageDialog {
            id: backToNormalDialog
            buttons: MessageDialog.Ok | MessageDialog.Cancel
            text: qsTr("Return to default mode")
            informativeText: qsTr("Do you want to back to default?")
            detailedText: qsTr("This action will reboot machine, please confirm.")
            onButtonClicked: function (button, role) {
                switch (button) {
                case MessageDialog.Ok:
                    TreeLandHelper.backToNormal()
                    TreeLandHelper.reboot()
                    break;
                }
            }
        }

        ColumnLayout {
            id: workspaceLoader
            anchors.fill: parent

            Row {
                id: topbar
                Layout.fillWidth: true

                Rectangle {
                    color: 'white'
                    anchors.fill: parent
                }

                Button {
                    id: optionsBtn
                    text: qsTr("Options")
                    onClicked: optionsMenu.open()

                    Menu {
                        id: optionsMenu
                        y: optionsBtn.height

                        Menu {
                            title: qsTr("Switch Windows Layout")
                            MenuItem {
                                text: "Stack Layout"
                                onClicked: {
                                    decorationManager.mode = XdgDecorationManager.DecidesByClient
                                    stackLayout.visible = true
                                }
                            }
                            MenuItem {
                                text: "Tiled Layout"
                                onClicked: {
                                    decorationManager.mode = XdgDecorationManager.PreferServerSide
                                    stackLayout.visible = false
                                }
                            }
                        }

                        MenuSeparator { }

                        Menu {
                            title: "Session"
                            MenuItem {
                                text: "Lock"
                                onClicked: {
                                    greeter.visible = true
                                }
                            }
                        }

                        MenuSeparator { }

                        MenuItem {
                            text: qsTr("Back to normal mode")
                            onClicked: {
                                backToNormalDialog.open()
                            }
                        }

                        MenuItem {
                            text: qsTr("Reboot")
                            onClicked: {
                                TreeLandHelper.reboot()
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                StackWorkspace {
                    id: stackLayout
                    visible: true
                    anchors.fill: parent
                    activeFocusItem: renderWindow.activeFocusItem
                }

                TiledWorkspace {
                    visible: !stackLayout.visible
                    anchors.fill: parent
                    activeFocusItem: renderWindow.activeFocusItem
                }
            }
        }

        Connections {
            target: TreeLandHelper
            function onGreeterVisibleChanged() {
                greeter.visible = true
            }
        }

        Greeter {
            id: greeter
            visible: !TreeLand.testMode
            anchors.fill: parent
        }
    }
}

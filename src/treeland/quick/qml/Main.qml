// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Waylib.Server
import TreeLand
import TreeLand.Utils
import TreeLand.Protocols
import TreeLand.Greeter

Item {
    id :root

    WaylandServer {
        id: server

        ShortcutManager {
            helper: Helper
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

        PersonalizationManager {
            id: personalizationManager
            Component.onCompleted: {
                TreeLand.personalManager = personalizationManager
            }
        }

        WaylandBackend {
            id: backend

            onOutputAdded: function(output) {
                if (!backend.hasDrm)
                    output.forceSoftwareCursor = true // Test

                Helper.allowNonDrmOutputAutoChangeMode(output)
                QmlHelper.outputManager.add({waylandOutput: output})
                outputManagerV1.newOutput(output)
                treelandOutputManager.newOutput(output)
            }
            onOutputRemoved: function(output) {
                output.OutputItem.item.invalidate()
                QmlHelper.outputManager.removeIf(function(prop) {
                    return prop.waylandOutput === output
                })
                outputManagerV1.removeOutput(output)
                treelandOutputManager.removeOutput(output)
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

                let clientName = Helper.clientName(surface.surface)
                if (!surface.isPopup && clientName !== "dde-desktop" && clientName !== "dde-launchpad") {
                    extForeignToplevelList.add(surface)
                    treelandForeignToplevelManager.add(surface)
                }
            }
            onSurfaceRemoved: function(surface) {
                QmlHelper.xdgSurfaceManager.removeIf(function(prop) {
                    return prop.waylandSurface === surface
                })

                let clientName = Helper.clientName(surface.surface)
                if (!surface.isPopup && (clientName !== "dde-desktop" || clientName !== "dde-launchpad")) {
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

            eventFilter: Helper
            keyboardFocus: Helper.getFocusSurfaceFrom(renderWindow.activeFocusItem)
        }

        GammaControlManager {
            onGammaChanged: function(output, gamma_control, ramp_size, r, g, b) {
                if (!output.setGammaLut(ramp_size, r, g, b)) {
                    sendFailedAndDestroy(gamma_control);
                };
            }
        }

        OutputManager {
            id: outputManagerV1

            onRequestTestOrApply: function(config, onlyTest) {
                var states = outputManagerV1.stateListPending()
                var ok = true

                for (const i in states) {
                    let output = states[i].output
                    output.enable(states[i].enabled)
                    if (states[i].enabled) {
                        if (states[i].mode)
                            output.setMode(states[i].mode)
                        else
                            output.setCustomMode(states[i].custom_mode_size,
                                                 states[i].custom_mode_refresh)

                        output.enableAdaptiveSync(states[i].adaptive_sync_enabled)
                        if (!onlyTest) {
                            let outputDelegate = output.OutputItem.item
                            outputDelegate.setTransform(states[i].transform)
                            outputDelegate.setScale(states[i].scale)
                            outputDelegate.x = states[i].x
                            outputDelegate.y = states[i].y
                        }
                    }

                    if (onlyTest) {
                        ok &= output.test()
                        output.rollback()
                    } else {
                        ok &= output.commit()
                    }
                }
                outputManagerV1.sendResult(config, ok)
            }
        }

        TreelandOutputManager {
            id: treelandOutputManager

            onRequestSetPrimaryOutput: function(outputName) {
                primaryOutput = outputName
            }
        }

        CursorShapeManager { }

        WaylandSocket {
            id: masterSocket

            freezeClientWhenDisable: false

            Component.onCompleted: {
                console.info("Listing on:", socketFile)
                Helper.socketFile = socketFile
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
                return prop.popupSurface === surface
            })
        }
    }

    OutputRenderWindow {
        id: renderWindow

        compositor: compositor
        width: QmlHelper.layout.implicitWidth
        height: QmlHelper.layout.implicitHeight

        property OutputDelegate activeOutputDelegate: null

        EventJunkman {
            anchors.fill: parent
        }

        Item {
            id: outputLayout

            DynamicCreatorComponent {
                id: outputDelegateCreator
                creator: QmlHelper.outputManager

                OutputDelegate {
                    id: outputDelegate
                    waylandCursor: cursor1
                    x: { x = QmlHelper.layout.implicitWidth }
                    y: 0

                    onLastActiveCursorItemChanged: {
                        if (lastActiveCursorItem != null)
                            renderWindow.activeOutputDelegate = outputDelegate
                        else if (renderWindow.activeOutputDelegate === outputDelegate) {
                            for (const output of QmlHelper.layout.outputs) {
                                if (output.lastActiveCursorItem) {
                                    renderWindow.activeOutputDelegate = output
                                    break
                                }
                            }
                        }
                    }

                    Component.onCompleted: {
                        if (renderWindow.activeOutputDelegate == null) {
                            renderWindow.activeOutputDelegate = outputDelegate
                        }
                    }

                    Component.onDestruction: {
                        if (renderWindow.activeOutputDelegate === outputDelegate) {
                            for (const output of QmlHelper.layout.outputs) {
                                if (output.lastActiveCursorItem && output !== outputDelegate) {
                                    renderWindow.activeOutputDelegate = output
                                    break
                                }
                            }
                        }
                    }
                }
            }

            Greeter {
                id: greeter
                z: 1
                visible: !TreeLand.testMode
                anchors.fill: renderWindow.activeOutputDelegate
            }
        }

        ColumnLayout {
            id: workspaceLoader
            anchors.fill: parent
            visible: !greeter.visible

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Loader {
                    id: stackLayout
                    anchors.fill: parent
                    sourceComponent: StackWorkspace {
                        activeOutputDelegate: renderWindow.activeOutputDelegate
                    }
                }

                Loader {
                    active: !stackLayout.active
                    anchors.fill: parent
                    sourceComponent: TiledWorkspace { }
                }
            }
        }

        Connections {
            target: socketProxy
            function onUserActivated(user) {
                Helper.currentUser = user
            }
        }

        Connections {
            target: Helper
            function onGreeterVisibleChanged() {
                greeter.visible = true
            }
        }
    }
}

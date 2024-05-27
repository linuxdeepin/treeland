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

        Seat {
            id: seat0
            name: "seat0"
            cursor: Cursor {
                id: cursor1

                layout: QmlHelper.layout
            }

            eventFilter: Helper
            keyboardFocus: Helper.activatedSurface?.surface || null
        }

        XdgShell {
            id: shell
            property int workspaceId: 0

            onSurfaceAdded: function(surface) {
                let type = surface.isPopup ? "popup" : "toplevel"
                // const wid = QmlHelper.workspaceManager.layoutOrder.get((++workspaceId) % QmlHelper.workspaceManager.layoutOrder.count).wsid
                const wid = QmlHelper.workspaceManager.layoutOrder.get(stackLayout.item.currentWorkspaceId).wsid
                QmlHelper.xdgSurfaceManager.add({type: type, workspaceId: wid, waylandSurface: surface})

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

        XWayland {
            id: xwayland
            compositor: compositor.compositor
            seat: seat0.seat
            lazy: false

            onReady: masterSocket.addClient(client())

            onSurfaceAdded: function(surface) {
                QmlHelper.xwaylandSurfaceManager.add({waylandSurface: surface, workspaceId: stackLayout.item.currentWorkspaceId})
                treelandForeignToplevelManager.add(surface)
            }
            onSurfaceRemoved: function(surface) {
                QmlHelper.xwaylandSurfaceManager.removeIf(function(prop) {
                    return prop.waylandSurface === surface
                })
                treelandForeignToplevelManager.remove(surface)
            }
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

        XdgOutputManager {
            id: xdgOutputManager
            layout: QmlHelper.layout
        }

        FractionalScaleManagerV1 {
            id: fractionalScaleManagerV1
        }

        ScreenCopyManager { }
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

        onOutputViewportInitialized: function (viewport) {
            // Trigger QWOutput::frame signal in order to ensure WOutputHelper::renderable
            // property is true, OutputRenderWindow when will render this output in next frame.
            Helper.enableOutput(viewport.output)
        }

        EventJunkman {
            anchors.fill: parent
            focus: false
        }

        onActiveFocusItemChanged: {
            // in case unexpected behavior happens
            if (activeFocusItem === renderWindow.contentItem) {
                console.warn('focusOnroot')
                // manually pick one child to focus
                for(let item of renderWindow.contentItem.children) {
                    if (item.focus) {
                        item.forceActiveFocus()
                        return
                    }
                }
                console.exception('no active focus !!!')
            }
        }

        Item {
            id: outputLayout
            objectName: "outputlayout"
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
        }

        FocusScope {
            id: workspaceLoader
            objectName: "workspaceLoader"
            anchors.fill: parent
            enabled: visible
            focus: enabled
            ColumnLayout {
                anchors.fill: parent
                FocusScope {
                    objectName: "loadercontainer"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    focus: true
                    activeFocusOnTab: true
                    Loader {
                        id: stackLayout
                        anchors.fill: parent
                        focus: true
                        sourceComponent: StackWorkspace {
                            focus: stackLayout.active
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
        }

        Loader {
            active: !TreeLand.testMode
            id: greeter
            asynchronous: true
            sourceComponent: Repeater {
                model: QmlHelper.layout.outputs
                delegate: Greeter {
                    property var output: modelData.output.OutputItem.item
                    property CoordMapper outputCoordMapper: this.CoordMapper.helper.get(output)
                    focus: true
                    anchors.fill: outputCoordMapper
                }
            }
        }

        Connections {
            target: greeter.active ? GreeterModel : null
            function onAnimationPlayFinished() {
                greeter.active = false
            }
        }

        Connections {
            target: Helper
            function onGreeterVisibleChanged() {
                greeter.active = !TreeLand.testMode
            }
        }

        Shortcut {
            sequences: ["Meta+S"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.multitaskViewToggled()
            }
        }
        Shortcut {
            sequences: ["Meta+L"]
            autoRepeat: false
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.screenLocked()
            }
        }
        Shortcut {
            sequences: ["Alt+Right"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.nextWorkspace()
            }
        }
        Shortcut {
            sequences: ["Alt+Left"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.prevWorkspace()
            }
        }
        Shortcut {
            sequences: ["Meta+Shift+Right"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.moveToNeighborWorkspace(1)
            }
        }
        Shortcut {
            sequences: ["Meta+Shift+Left"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.moveToNeighborWorkspace(-1)
            }
        }
        Connections {
            target: QmlHelper.shortcutManager
            function onScreenLocked() {
                greeter.active = !TreeLand.testMode
            }
        }
    }
}

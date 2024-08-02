// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Waylib.Server
import TreeLand
import TreeLand.Utils
import TreeLand.Protocols

SurfaceItemFactory {
    id: root

    required property DynamicCreatorComponent creatorCompoment
    required property ToplevelSurface wSurface
    required property string type
    required property int wid

    property var doDestroy: helper.doDestroy
    property var cancelMinimize: helper.cancelMinimize
    property var personalizationMapper: PersonalizationV1.Attached(wSurface)
    property int outputCounter: 0
    property alias decoration: decoration
    readonly property XWaylandSurfaceItem asXwayland: {surfaceItem as XWaylandSurfaceItem}
    readonly property XdgSurfaceItem asXdg: {surfaceItem as XdgSurfaceItem}
    readonly property bool hasDecoration: decoration.enable && !helper.isFullScreen
    z: {
        if (Helper.clientName(wSurface.surface) === "dde-desktop") {
            return -100 + 1
        }
        else {
            return 0
        }
    }

    function move(pos) {
        manualMoveResizing = true
        surfaceItem.x = pos.x
        surfaceItem.y = pos.y
        manualMoveResizing = false
    }

    // put here, otherwise may initialize late
    parent: QmlHelper.workspaceManager.workspacesById.get(wid)

    surfaceItem {
        parent: root.parent
        shellSurface: wSurface
        topPadding: hasDecoration ? decoration.topMargin : 0
        bottomPadding: hasDecoration ? decoration.bottomMargin : 0
        leftPadding: hasDecoration ? decoration.leftMargin : 0
        rightPadding: hasDecoration ? decoration.rightMargin : 0
        focus: wSurface === Helper.activatedSurface
        resizeMode:
            if (!surfaceItem.effectiveVisible)
                SurfaceItem.ManualResize
            else if (stateTransition.running
                    || wSurface.isMaximized)
                SurfaceItem.SizeToSurface
            else
                SurfaceItem.SizeFromSurface
        onResizeModeChanged: {
            // if surface mapped when not visible, it will change mode to sizefromsurf
            // but mode change is not applied if no resize event happens afterwards, so trigger resize here
            if (surfaceItem.resizeMode != SurfaceItem.ManualResize)
                surfaceItem.resize(surfaceItem.resizeMode)
        }
        onEffectiveVisibleChanged: {
            if (surfaceItem.effectiveVisible) {
                console.assert(surfaceItem.resizeMode !== SurfaceItem.ManualResize,
                               "The surface's resizeMode Shouldn't is ManualResize")
                // Apply the WSurfaceItem's size to wl_surface
                surfaceItem.resize(SurfaceItem.SizeToSurface)
            } else {
                Helper.cancelMoveResize(surfaceItem)
            }
        }
        transitions: Transition {
            id: stateTransition
            NumberAnimation {
                properties: "x,y,width,height"
                duration: 100
            }
        }
        states: [
            State {
                name: "" // avoid applying propertychange on initialize
                PropertyChanges {
                    id: defulatState
                    restoreEntryValues: false
                    target: surfaceItem
                    x: store.normal.x
                    y: store.normal.y
                    width: store.normal.width
                    height: store.normal.height
                }
            },
            State {
                name: "intermediate"
                PropertyChanges {
                    restoreEntryValues: false
                    target: surfaceItem
                    x: store.normal.x
                    y: store.normal.y
                    width: store.normal.width
                    height: store.normal.height
                }
            },
            State {
                name: "maximize"
                when: helper.isMaximize
                PropertyChanges {
                    restoreEntryValues: true
                    target: surfaceItem
                    x: helper.maximizeRect.x
                    y: helper.maximizeRect.y
                    width: helper.maximizeRect.width
                    height: helper.maximizeRect.height
                }
            },
            State {
                name: "fullscreen"
                when: helper.isFullScreen
                PropertyChanges {
                    restoreEntryValues: true
                    target: surfaceItem
                    x: helper.fullscreenRect.x
                    y: helper.fullscreenRect.y
                    z: 100 + 1 // LayerType.Overlay + 1
                    width: helper.fullscreenRect.width
                    height: helper.fullscreenRect.height
                }
            }
        ]

        delegate: Item {
            required property SurfaceItem surface

            anchors.fill: parent
            SurfaceItemContent {
                id: content
                surface: parent.surface.surface
                anchors.fill: parent
            }

            Loader {
                id: effectLoader
                active: root.cornerRadius > 0 && decoration.enable
                anchors.fill: parent
                // TODO: Use QSGGeometry(like as Rectangle, Qt 6.8 supports topLeftRadius)
                // to clip texture by vertex shader.
                MultiEffect {
                    anchors.fill: parent
                    source: ShaderEffectSource {
                        sourceItem: content
                        width: content.width
                        height: content.height
                        live: true
                        hideSource: true
                    }

                    maskSource: ShaderEffectSource {
                        width: content.width
                        height: content.height
                        samples: 4
                        sourceItem: Item {
                            width: content.width
                            height: content.height
                            Rectangle {
                                anchors {
                                    fill: parent
                                    topMargin: -radius
                                }
                                radius: Math.min(root.cornerRadius, content.width / 2, content.height)
                                antialiasing: true
                            }
                        }
                    }

                    maskEnabled: true
                }
            }
        }
    }

    property var store: ({})
    property int storeNormalWidth: undefined
    property bool isRestoring: false
    property bool aboutToRestore: false
    readonly property real cornerRadius: helper.isMaximize  ? 0 : 15

    function saveState() {
        let nw = store ?? {}
        console.debug(`store = ${store} state = {x: ${surfaceItem.x}, y: ${surfaceItem.y}, width: ${surfaceItem.width}, height: ${surfaceItem.height}}`)
        nw.normal = {x: surfaceItem.x, y: surfaceItem.y, width: surfaceItem.width, height: surfaceItem.height}
        store = nw
    }
    function restoreState(toStore) {
        console.debug(`restoring state to ${QmlHelper.printStructureObject(toStore.normal)}`)
        surfaceItem.state = "intermediate"
        store = toStore
        surfaceItem.state = ""
    }
    onIsMoveResizingChanged: if (!isMoveResizing) {
        if (state === "")
            saveState()
    }
    onStoreChanged: {
        storeNormalWidth = store.normal?.width ?? 0
    }
    Component.onCompleted: {
        saveState() // save initial state
    }

    OutputLayoutItem {
        parent: surfaceItem
        anchors.fill: parent
        layout: Helper.outputLayout

        onEnterOutput: function(output) {
            if (wSurface.surface) {
                wSurface.surface.enterOutput(output)
            }
            Helper.onSurfaceEnterOutput(wSurface, surfaceItem, output)
            outputCounter++

            if (outputCounter === 1) {
                const pos = QmlHelper.winposManager.nextPos(wSurface.appId, surfaceItem.parent, surfaceItem)
                let outputDelegate = output.OutputItem.item
                move(pos)

                if (Helper.clientName(wSurface.surface) === "dde-desktop") {
                    surfaceItem.x = outputDelegate.x
                    surfaceItem.y = outputDelegate.y
                    surfaceItem.width = outputDelegate.width
                    surfaceItem.height = outputDelegate.height
                }
            }
        }
        onLeaveOutput: function(output) {
            if (wSurface && wSurface.surface)
                wSurface.surface.leaveOutput(output)
            Helper.onSurfaceLeaveOutput(wSurface, surfaceItem, output)
            outputCounter--

            if (outputCounter == 0 && helper.mapped) {
                const pos = QmlHelper.winposManager.nextPos(wSurface.appId, surfaceItem.parent, surfaceItem)
                move(pos)
            }
        }
    }

    WindowDecoration {
        id: decoration
        parent: surfaceItem
        anchors.fill: parent
        z: SurfaceItem.ZOrder.ContentItem - 1
        surface: wSurface
        visible: hasDecoration
        moveable: !helper.isMaximize && !helper.isFullScreen
        radius: cornerRadius
    }

    StackToplevelHelper {
        id: helper
        surface: surfaceItem
        waylandSurface: wSurface
        creator: creatorCompoment
        decoration: decoration
        surfaceWrapper: root
    }

    property bool manualMoveResizing: false
    property bool isMoveResizing: manualMoveResizing || Helper.resizingItem === surfaceItem || Helper.movingItem === surfaceItem
}

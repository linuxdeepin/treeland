// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import TreeLand
import TreeLand.Protocols
import TreeLand.Utils
import TreeLand.Protocols
import org.deepin.dtk 1.0 as D

Item {
    id: root
    function getSurfaceItemFromWaylandSurface(surface) {
        let finder = function(props) {
            if (!props.waylandSurface)
                return false
            // surface is WToplevelSurface or WSurfce
            if (props.waylandSurface === surface || props.waylandSurface.surface === surface)
                return true
        }

        let toplevel = QmlHelper.xdgSurfaceManager.getIf(toplevelComponent, finder)
        if (toplevel) {
            return {
                shell: toplevel,
                item: toplevel,
                type: "toplevel"
            }
        }

        let popup = QmlHelper.xdgSurfaceManager.getIf(popupComponent, finder)
        if (popup) {
            return {
                shell: popup,
                item: popup.xdgSurface,
                type: "popup"
            }
        }

        let layer = QmlHelper.layerSurfaceManager.getIf(layerComponent, finder)
        if (layer) {
            return {
                shell: layer,
                item: layer.surfaceItem,
                type: "layer"
            }
        }

        let xwayland = QmlHelper.xwaylandSurfaceManager.getIf(xwaylandComponent, finder)
        if (xwayland) {
            return {
                shell: xwayland,
                item: xwayland,
                type: "xwayland"
            }
        }

        return null
    }

    required property OutputDelegate activeOutputDelegate
    readonly property real switcherHideOpacity: 0.3
    Item {
        // DONE: some surfaces like layersurface should not be opacity
        id: allWins
        anchors.fill: parent
        enabled: !switcher.visible && !multitaskView.active
        opacity: if (switcher.visible) switcherHideOpacity
            else 1
        z: 0

        DynamicCreatorComponent {
            id: toplevelComponent
            creator: QmlHelper.xdgSurfaceManager
            chooserRole: "type"
            chooserRoleValue: "toplevel"
            autoDestroy: false

            onObjectRemoved: function (obj) {
                obj.doDestroy()
            }

            XdgSurface {
                id: toplevelSurfaceItem

                property var doDestroy: helper.doDestroy
                property var cancelMinimize: helper.cancelMinimize
                property var surfaceDecorationMapper: toplevelSurfaceItem.waylandSurface.XdgDecorationManager
                property var personalizationMapper: toplevelSurfaceItem.waylandSurface.PersonalizationManager
                property int outputCounter: 0
                z: {
                    if (Helper.clientName(waylandSurface.surface) === "dde-desktop") {
                        return -100 + 1
                    }
                    else if (Helper.clientName(waylandSurface.surface) === "dde-launchpad") {
                        return 25
                    }
                    else {
                        return 0
                    }
                }
                // opacity: switcher.visible&&!activeFocus?switcherHideOpacity:1

                topPadding: decoration.enable ? decoration.topMargin : 0
                bottomPadding: decoration.enable ? decoration.bottomMargin : 0
                leftPadding: decoration.enable ? decoration.leftMargin : 0
                rightPadding: decoration.enable ? decoration.rightMargin : 0

                OutputLayoutItem {
                    anchors.fill: parent
                    layout: QmlHelper.layout

                    onEnterOutput: function(output) {
                        if (waylandSurface.surface) {
                            waylandSurface.surface.enterOutput(output)
                        }
                        Helper.onSurfaceEnterOutput(waylandSurface, toplevelSurfaceItem, output)
                        outputCounter++

                        if (outputCounter == 1) {
                            let outputDelegate = output.OutputItem.item
                            toplevelSurfaceItem.x = outputDelegate.x
                                    + Helper.getLeftExclusiveMargin(waylandSurface)
                                    + 10
                            toplevelSurfaceItem.y = outputDelegate.y
                                    + Helper.getTopExclusiveMargin(waylandSurface)
                                    + 10

                            if (Helper.clientName(waylandSurface.surface) === "dde-desktop") {
                                toplevelSurfaceItem.x = outputDelegate.x
                                toplevelSurfaceItem.y = outputDelegate.y
                                toplevelSurfaceItem.width = output.size.width
                                toplevelSurfaceItem.height = output.size.height
                            }

                            if (Helper.clientName(waylandSurface.surface) === "dde-launchpad") {
                                toplevelSurfaceItem.x = outputDelegate.x
                                toplevelSurfaceItem.y = outputDelegate.y
                            }
                        }
                    }
                    onLeaveOutput: function(output) {
                        waylandSurface.surface.leaveOutput(output)
                        Helper.onSurfaceLeaveOutput(waylandSurface, toplevelSurfaceItem, output)
                        outputCounter--
                    }
                }

                WindowDecoration {
                    property var enable: surfaceDecorationMapper.serverDecorationEnabled

                    id: decoration
                    anchors.fill: parent
                    z: toplevelSurfaceItem.contentItem.z - 1
                    surface: toplevelSurfaceItem.waylandSurface
                    visible: enable
                }

                StackToplevelHelper {
                    id: helper
                    surface: toplevelSurfaceItem
                    waylandSurface: toplevelSurfaceItem.waylandSurface
                    switcherModel: switcher.model
                    dockPreviewModel: dockPreview.model
                    creator: toplevelComponent
                    decoration: decoration
                }

                states: [
                    State {
                        name: "maximize"
                        when: helper.isMaximize
                        PropertyChanges {
                            restoreEntryValues: true
                            target: toplevelSurfaceItem
                            x: helper.getMaximizeX()
                            y: helper.getMaximizeY()
                            width: helper.getMaximizeWidth()
                            height: helper.getMaximizeHeight()
                        }
                    },
                    State {
                        name: "fullscreen"
                        when: helper.isFullScreen
                        PropertyChanges {
                            restoreEntryValues: true
                            target: toplevelSurfaceItem
                            x: helper.getFullscreenX()
                            y: helper.getFullscreenY()
                            z: Helper.clientName(waylandSurface.surface) == "dde-launchpad" ? 25 : 100 + 1 // LayerType.Overlay + 1
                            width: helper.getFullscreenWidth()
                            height: helper.getFullscreenHeight()
                        }
                    }
                ]

                ShaderEffectSource {
                    id: background
                    z: toplevelSurfaceItem.contentItem.z - 2
                    visible: personalizationMapper.backgroundType
                    anchors.fill: parent
                    sourceRect: { Qt.rect(parent.x, parent.y, parent.width, parent.height) }
                    sourceItem: personalizationMapper.backgroundImage
                }
            }
        }

        DynamicCreatorComponent {
            id: popupComponent
            creator: QmlHelper.xdgSurfaceManager
            chooserRole: "type"
            chooserRoleValue: "popup"

            Popup {
                id: popup

                required property WaylandXdgSurface waylandSurface
                property string type

                property alias xdgSurface: popupSurfaceItem
                property var parentItem: root.getSurfaceItemFromWaylandSurface(waylandSurface.parentSurface)

                parent: parentItem ? parentItem.item : root
                visible: parentItem && parentItem.item.effectiveVisible
                        && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
                x: {
                    let retX = 0 // X coordinate relative to parent
                    let minX = 0
                    let maxX = root.width - xdgSurface.width
                    if (!parentItem) {
                        retX = popupSurfaceItem.implicitPosition.x
                        if (retX > maxX)
                            retX = maxX
                        if (retX < minX)
                            retX = minX
                    } else {
                        retX = popupSurfaceItem.implicitPosition.x / parentItem.item.surfaceSizeRatio + parentItem.item.contentItem.x
                        let parentX = parent.mapToItem(root, 0, 0).x
                        if (retX + parentX > maxX) {
                            if (parentItem.type === "popup")
                                retX = retX - xdgSurface.width - parent.width
                            else
                                retX = maxX - parentX
                        }
                        if (retX + parentX < minX)
                            retX = minX - parentX
                    }
                    return retX
                }
                y: {
                    let retY = 0 // Y coordinate relative to parent
                    let minY = 0
                    let maxY = root.height - xdgSurface.height
                    if (!parentItem) {
                        retY = popupSurfaceItem.implicitPosition.y
                        if (retY > maxY)
                            retY = maxY
                        if (retY < minY)
                            retY = minY
                    } else {
                        retY = popupSurfaceItem.implicitPosition.y / parentItem.item.surfaceSizeRatio + parentItem.item.contentItem.y
                        let parentY = parent.mapToItem(root, 0, 0).y
                        if (retY + parentY > maxY)
                            retY = maxY - parentY
                        if (retY + parentY < minY)
                            retY = minY - parentY
                    }
                    return retY
                }
                padding: 0
                background: null
                closePolicy: Popup.NoAutoClose

                XdgSurface {
                    id: popupSurfaceItem
                    waylandSurface: popup.waylandSurface

                    OutputLayoutItem {
                        anchors.fill: parent
                        layout: QmlHelper.layout

                        onEnterOutput: function(output) {
                            if (waylandSurface.surface) {
                                waylandSurface.surface.enterOutput(output)
                            }
                            Helper.onSurfaceEnterOutput(waylandSurface, popupSurfaceItem, output)
                        }
                        onLeaveOutput: function(output) {
                            waylandSurface.surface.leaveOutput(output)
                            Helper.onSurfaceLeaveOutput(waylandSurface, popupSurfaceItem, output)
                        }
                    }
                }
            }
            D.InWindowBlur {
                id: blur
                anchors.fill: parent
                z: toplevelSurfaceItem.contentItem.z - 2
                visible: personalizationMapper.backgroundType == 2
                radius: 16
            }
        }

        DynamicCreatorComponent {
            id: xwaylandComponent
            creator: QmlHelper.xwaylandSurfaceManager
            autoDestroy: false

            onObjectRemoved: function (obj) {
                obj.doDestroy()
            }

            XWaylandSurfaceItem {
                id: xwaylandSurfaceItem

                required property XWaylandSurface waylandSurface
                property var doDestroy: helper.doDestroy
                property var cancelMinimize: helper.cancelMinimize
                property var surfaceParent: root.getSurfaceItemFromWaylandSurface(waylandSurface.parentXWaylandSurface)
                property int outputCounter: 0

                surface: waylandSurface
                parentSurfaceItem: surfaceParent ? surfaceParent.item : null
                z: waylandSurface.bypassManager ? 1 : 0 // TODO: make to enum type
                positionMode: {
                    if (!xwaylandSurfaceItem.effectiveVisible)
                        return XWaylandSurfaceItem.ManualPosition

                    return (Helper.movingItem === xwaylandSurfaceItem || resizeMode === SurfaceItem.SizeToSurface)
                            ? XWaylandSurfaceItem.PositionToSurface
                            : XWaylandSurfaceItem.PositionFromSurface
                }
                // opacity: switcher.visible&&!activeFocus?switcherHideOpacity:1

                topPadding: decoration.enable ? decoration.topMargin : 0
                bottomPadding: decoration.enable ? decoration.bottomMargin : 0
                leftPadding: decoration.enable ? decoration.leftMargin : 0
                rightPadding: decoration.enable ? decoration.rightMargin : 0

                surfaceSizeRatio: {
                    const po = waylandSurface.surface.primaryOutput
                    if (!po)
                        return 1.0
                    if (bufferScale >= po.scale)
                        return 1.0
                    return po.scale / bufferScale
                }

                onEffectiveVisibleChanged: {
                    if (xwaylandSurfaceItem.effectiveVisible)
                        xwaylandSurfaceItem.move(XWaylandSurfaceItem.PositionToSurface)
                }

                // TODO: ensure the event to WindowDecoration before WSurfaceItem::eventItem on surface's edges
                // maybe can use the SinglePointHandler?
                WindowDecoration {
                    id: decoration

                    property bool enable: !waylandSurface.bypassManager
                                        && waylandSurface.decorationsType !== XWaylandSurface.DecorationsNoBorder

                    anchors.fill: parent
                    z: xwaylandSurfaceItem.contentItem.z - 1
                    visible: enable
                    surface: waylandSurface
                }

                OutputLayoutItem {
                    anchors.fill: parent
                    layout: QmlHelper.layout

                    onEnterOutput: function(output) {
                        if (xwaylandSurfaceItem.waylandSurface.surface)
                            xwaylandSurfaceItem.waylandSurface.surface.enterOutput(output);
                        Helper.onSurfaceEnterOutput(waylandSurface, xwaylandSurfaceItem, output)

                        outputCounter++

                        if (outputCounter == 1) {
                            let outputDelegate = output.OutputItem.item
                            xwaylandSurfaceItem.x = outputDelegate.x
                                    + Helper.getLeftExclusiveMargin(waylandSurface)
                                    + 10
                            xwaylandSurfaceItem.y = outputDelegate.y
                                    + Helper.getTopExclusiveMargin(waylandSurface)
                                    + 10
                        }
                    }
                    onLeaveOutput: function(output) {
                        if (xwaylandSurfaceItem.waylandSurface.surface)
                            xwaylandSurfaceItem.waylandSurface.surface.leaveOutput(output);
                        Helper.onSurfaceLeaveOutput(waylandSurface, xwaylandSurfaceItem, output)
                        outputCounter--
                    }
                }

                StackToplevelHelper {
                    id: helper
                    surface: xwaylandSurfaceItem
                    waylandSurface: xwaylandSurfaceItem.waylandSurface
                    creator: xwaylandComponent
                    decoration: decoration
                }

                states: [
                    State {
                        name: "maximize"
                        when: helper.isMaximize
                        PropertyChanges {
                            restoreEntryValues: true
                            target: xwaylandSurfaceItem
                            x: helper.getMaximizeX()
                            y: helper.getMaximizeY()
                            width: helper.getMaximizeWidth()
                            height: helper.getMaximizeHeight()
                            positionMode: XWaylandSurfaceItem.PositionToSurface
                        }
                    },
                    State {
                        name: "fullscreen"
                        when: helper.isFullScreen
                        PropertyChanges {
                            restoreEntryValues: true
                            target: xwaylandSurfaceItem
                            x: helper.getFullscreenX()
                            y: helper.getFullscreenY()
                z: 100 + 1 // LayerType.Overlay + 1
                            width: helper.getFullscreenWidth()
                            height: helper.getFullscreenHeight()
                            positionMode: XWaylandSurfaceItem.PositionToSurface
                        }
                        PropertyChanges {
                            restoreEntryValues: true
                            target: decoration
                            enable: false
                        }
                    }
                ]
            }
        }

        DynamicCreatorComponent {
            id: inputPopupComponent
            creator: QmlHelper.inputPopupSurfaceManager

            InputPopupSurface {
                required property InputMethodHelper inputMethodHelper
                required property WaylandInputPopupSurface popupSurface

                id: inputPopupSurface
                surface: popupSurface
                helper: inputMethodHelper
            }
        }
    }

    DynamicCreatorComponent {
        id: layerComponent
        creator: QmlHelper.layerSurfaceManager
        autoDestroy: false

        onObjectRemoved: function (obj) {
            obj.doDestroy()
        }

        LayerSurface {
            id: layerSurface
            creator: layerComponent
        }
    }

    WindowsSwitcher {
        id: switcher
        z: 100 + 1
        anchors.fill: parent
        visible: false // dbgswtchr.checked
        activeOutput: activeOutputDelegate
        onSurfaceActivated: (surface) => {
            surface.cancelMinimize()
            Helper.activatedSurface = surface.waylandSurface
        }
    }

    Loader {
        id: multitaskView
        active: false
        sourceComponent: Component {
            MultitaskView {
                anchors.fill: parent
                model: switcher.model
                onVisibleChanged: {
                    console.assert(!visible,'should be exit')
                    multitaskView.active = false
                }
            }
        }
    }

    Connections {
        target: treelandForeignToplevelManager
        function onRequestDockPreview(surfaces, target, abs, direction) {
            dockPreview.show(surfaces, getSurfaceItemFromWaylandSurface(target), abs, direction)
        }
        function onRequestDockClose() {
            dockPreview.close()
        }
    }

    DockPreview {
        id: dockPreview
        z: 100 + 1
        anchors.fill: parent
        visible: false
        onEntered: (relativeSurface) => {
            treelandForeignToplevelManager.enterDockPreview(relativeSurface)
        }
        onExited: (relativeSurface) => {
            treelandForeignToplevelManager.leaveDockPreview(relativeSurface)
        }
        onSurfaceActivated: (surface) => {
            surface.cancelMinimize()
            Helper.activatedSurface = surface.waylandSurface
        }
    }

    Connections {
        target: Helper
        function onSwitcherChanged(mode) {
            switch (mode) {
            case (Helper.Show):
                switcher.visible = true
                break
            case (Helper.Hide):
                switcher.visible = false
                break
            case (Helper.Next):
                switcher.next()
                break
            case (Helper.Previous):
                switcher.previous()
                break
            }
        }
    }
}

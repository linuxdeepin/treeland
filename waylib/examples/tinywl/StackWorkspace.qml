// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Tinywl

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

        let toplevel = Helper.xdgShellCreator.getIf(toplevelComponent, finder)
        if (toplevel) {
            return {
                shell: toplevel,
                item: toplevel,
                type: "toplevel"
            }
        }

        let popup = Helper.xdgShellCreator.getIf(popupComponent, finder)
        if (popup) {
            return {
                shell: popup,
                item: popup.xdgSurface,
                type: "popup"
            }
        }

        let layer = Helper.layerShellCreator.getIf(layerComponent, finder)
        if (layer) {
            return {
                shell: layer,
                item: layer.surfaceItem,
                type: "layer"
            }
        }

        let xwayland = Helper.xwaylandCreator.getIf(xwaylandComponent, finder)
        if (xwayland) {
            return {
                shell: xwayland,
                item: xwayland,
                type: "xwayland"
            }
        }

        return null
    }

    MiniDock {
        id: dock
        anchors {
            top: parent.top
            left: parent.left
            bottom: parent.bottom
            margins: 8
        }
        width: 250
    }

    DynamicCreatorComponent {
        id: toplevelComponent
        creator: Helper.xdgShellCreator
        chooserRole: "type"
        chooserRoleValue: "toplevel"
        autoDestroy: false

        onObjectRemoved: function (obj) {
            obj.doDestroy()
        }

        // TODO: Support server decoration
        XdgSurface {
            id: toplevelSurfaceItem
            property var doDestroy: helper.doDestroy
            property var cancelMinimize: helper.cancelMinimize
            property int outputCounter: 0

            topPadding: decoration.visible ? decoration.topMargin : 0
            bottomPadding: decoration.visible ? decoration.bottomMargin : 0
            leftPadding: decoration.visible ? decoration.leftMargin : 0
            rightPadding: decoration.visible ? decoration.rightMargin : 0

            WindowDecoration {
                id: decoration
                anchors.fill: parent
                z: SurfaceItem.ZOrder.ContentItem - 1
                surface: waylandSurface

                Connections {
                    target: Helper.xdgDecorationManager

                    function onSurfaceModeChanged(surface, mode) {
                        if (waylandSurface === surface)
                            visible = (mode !== XdgDecorationManager.Client)
                    }
                }

                Component.onCompleted: {
                    visible = Helper.xdgDecorationManager.modeBySurface(surface.surface) !== XdgDecorationManager.Client
                }
            }

            OutputLayoutItem {
                anchors.fill: parent
                layout: Helper.outputLayout

                onEnterOutput: function(output) {
                    waylandSurface.surface.enterOutput(output)
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
                    }
                }
                onLeaveOutput: function(output) {
                    waylandSurface.surface.leaveOutput(output)
                    Helper.onSurfaceLeaveOutput(waylandSurface, toplevelSurfaceItem, output)
                    outputCounter--
                }
            }

            StackToplevelHelper {
                id: helper
                surface: toplevelSurfaceItem
                waylandSurface: toplevelSurfaceItem.waylandSurface
                dockModel: dock.model
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
                        z: 100 + 1 // LayerType.Overlay + 1
                        width: helper.getFullscreenWidth()
                        height: helper.getFullscreenHeight()
                    }
                }
            ]
        }
    }

    DynamicCreatorComponent {
        id: popupComponent
        creator: Helper.xdgShellCreator
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
                    layout: Helper.outputLayout

                    onEnterOutput: function(output) {
                        waylandSurface.surface.enterOutput(output)
                        Helper.onSurfaceEnterOutput(waylandSurface, popupSurfaceItem, output)
                    }
                    onLeaveOutput: function(output) {
                        waylandSurface.surface.leaveOutput(output)
                        Helper.onSurfaceLeaveOutput(waylandSurface, popupSurfaceItem, output)
                    }
                }
            }
        }
    }

    DynamicCreatorComponent {
        id: layerComponent
        creator: Helper.layerShellCreator
        autoDestroy: false

        onObjectRemoved: function (obj) {
            obj.doDestroy()
        }

        LayerSurface {
            id: layerSurface
            creator: layerComponent
        }
    }

    DynamicCreatorComponent {
        id: xwaylandComponent
        creator: Helper.xwaylandCreator
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

            shellSurface: waylandSurface
            parentSurfaceItem: surfaceParent ? surfaceParent.item : null
            z: waylandSurface.bypassManager ? 1 : 0 // TODO: make to enum type
            positionMode: {
                if (!xwaylandSurfaceItem.effectiveVisible)
                    return XWaylandSurfaceItem.ManualPosition

                return (Helper.movingItem === xwaylandSurfaceItem || resizeMode === SurfaceItem.SizeToSurface)
                        ? XWaylandSurfaceItem.PositionToSurface
                        : XWaylandSurfaceItem.PositionFromSurface
            }

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
                z: SurfaceItem.ZOrder.ContentItem - 1
                visible: enable
                surface: waylandSurface
            }

            OutputLayoutItem {
                anchors.fill: parent
                layout: Helper.outputLayout

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
                dockModel: dock.model
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
        creator: Helper.inputPopupCreator

        InputPopupSurface {
            required property WaylandInputPopupSurface popupSurface

            parent: getSurfaceItemFromWaylandSurface(popupSurface.parentSurface)
            id: inputPopupSurface
            shellSurface: popupSurface
        }
    }
}

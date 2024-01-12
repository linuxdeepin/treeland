// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server
import TreeLand
import TreeLand.Protocols
import TreeLand.Utils

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

    GridLayout {
        anchors.fill: parent
        columns: Math.floor(root.width / 1920 * 4)

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

                resizeMode: SurfaceItem.SizeToSurface
                z: (waylandSurface && waylandSurface.isActivated) ? 1 : 0

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Math.max(toplevelSurfaceItem.minimumSize.width, 100)
                Layout.minimumHeight: Math.max(toplevelSurfaceItem.minimumSize.height, 50)
                Layout.maximumWidth: toplevelSurfaceItem.maximumSize.width
                Layout.maximumHeight: toplevelSurfaceItem.maximumSize.height
                Layout.horizontalStretchFactor: 1
                Layout.verticalStretchFactor: 1

                OutputLayoutItem {
                    anchors.fill: parent
                    layout: QmlHelper.layout

                    onEnterOutput: function(output) {
                        waylandSurface.surface.enterOutput(output)
                        TreeLandHelper.onSurfaceEnterOutput(waylandSurface, toplevelSurfaceItem, output)
                    }
                    onLeaveOutput: function(output) {
                        waylandSurface.surface.leaveOutput(output)
                        TreeLandHelper.onSurfaceLeaveOutput(waylandSurface, toplevelSurfaceItem, output)
                    }
                }

                TiledToplevelHelper {
                    id: helper

                    surface: toplevelSurfaceItem
                    waylandSurface: toplevelSurfaceItem.waylandSurface
                    creator: toplevelComponent
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
                closePolicy: Popup.CloseOnPressOutside

                XdgSurface {
                    id: popupSurfaceItem
                    waylandSurface: popup.waylandSurface

                    OutputLayoutItem {
                        anchors.fill: parent
                        layout: QmlHelper.layout

                        onEnterOutput: function(output) {
                            waylandSurface.surface.enterOutput(output)
                            TreeLandHelper.onSurfaceEnterOutput(waylandSurface, popupSurfaceItem, output)
                        }
                        onLeaveOutput: function(output) {
                            waylandSurface.surface.leaveOutput(output)
                            TreeLandHelper.onSurfaceLeaveOutput(waylandSurface, popupSurfaceItem, output)
                        }
                    }
                }

                onClosed: {
                    if (waylandSurface)
                        waylandSurface.surface.unmap()
                }
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

                surface: waylandSurface
                resizeMode: SurfaceItem.SizeToSurface
                // TODO: Support popup/menu
                positionMode: xwaylandSurfaceItem.effectiveVisible ? XWaylandSurfaceItem.PositionToSurface : XWaylandSurfaceItem.ManualPosition
                z: (waylandSurface && waylandSurface.isActivated) ? 1 : 0

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Math.max(xwaylandSurfaceItem.minimumSize.width, 100)
                Layout.minimumHeight: Math.max(xwaylandSurfaceItem.minimumSize.height, 50)
                Layout.maximumWidth: xwaylandSurfaceItem.maximumSize.width
                Layout.maximumHeight: xwaylandSurfaceItem.maximumSize.height
                Layout.horizontalStretchFactor: 1
                Layout.verticalStretchFactor: 1

                OutputLayoutItem {
                    anchors.fill: parent
                    layout: QmlHelper.layout

                    onEnterOutput: function(output) {
                        if (xwaylandSurfaceItem.waylandSurface.surface)
                            xwaylandSurfaceItem.waylandSurface.surface.enterOutput(output);
                        TreeLandHelper.onSurfaceEnterOutput(waylandSurface, xwaylandSurfaceItem, output)
                    }
                    onLeaveOutput: function(output) {
                        if (xwaylandSurfaceItem.waylandSurface.surface)
                            xwaylandSurfaceItem.waylandSurface.surface.leaveOutput(output);
                        TreeLandHelper.onSurfaceLeaveOutput(waylandSurface, xwaylandSurfaceItem, output)
                    }
                }

                TiledToplevelHelper {
                    id: helper

                    surface: xwaylandSurfaceItem
                    waylandSurface: surface.waylandSurface
                    creator: xwaylandComponent
                }
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

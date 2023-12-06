// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

Item {
    id: root
    required property Item activeFocusItem
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
                item: toplevel
            }
        }

        let layer = QmlHelper.layerSurfaceManager.getIf(layerComponent, finder)
        if (layer) {
            return {
                shell: layer,
                item: layer.surfaceItem
            }
        }

        let popup = QmlHelper.xdgSurfaceManager.getIf(popupComponent, finder)
        if (popup) {
            return {
                shell: popup,
                item: popup.xdgSurface
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
                id: surface

                property var doDestroy: helper.doDestroy

                resizeMode: SurfaceItem.SizeToSurface
                z: (waylandSurface && waylandSurface.isActivated) ? 1 : 0

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Math.max(surface.minimumSize.width, 100)
                Layout.minimumHeight: Math.max(surface.minimumSize.height, 50)
                Layout.maximumWidth: surface.maximumSize.width
                Layout.maximumHeight: surface.maximumSize.height
                Component.onCompleted: {
                    if (Layout.horizontalStretchFactor !== undefined) {
                        // introduced in Qt 6.5
                        Layout.horizontalStretchFactor = 1
                    }
                    if (Layout.verticalStretchFactor !== undefined) {
                        Layout.verticalStretchFactor = 1
                    }
                }

                TiledToplevelHelper {
                    id: helper

                    surface: surface
                    waylandSurface: surface.waylandSurface
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

                property alias xdgSurface: surface
                property var parentItem: root.getSurfaceItemFromWaylandSurface(waylandSurface.parentSurface)

                parent: parentItem ? parentItem.shell : root
                visible: parentItem && parentItem.item.effectiveVisible
                        && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
                x: {
                    if (!parentItem)
                        return surface.implicitPosition.x
                    return surface.implicitPosition.x / parentItem.item.surfaceSizeRatio + parentItem.item.contentItem.x
                }
                y: {
                    if (!parentItem)
                        return surface.implicitPosition.y
                    return surface.implicitPosition.y / parentItem.item.surfaceSizeRatio + parentItem.item.contentItem.y
                }
                padding: 0
                background: null
                closePolicy: Popup.CloseOnPressOutside

                XdgSurface {
                    id: surface
                    waylandSurface: popup.waylandSurface
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
                id: surface

                required property XWaylandSurface waylandSurface
                property var doDestroy: helper.doDestroy

                surface: waylandSurface
                resizeMode: SurfaceItem.SizeToSurface
                // TODO: Support popup/menu
                positionMode: surface.effectiveVisible ? XWaylandSurfaceItem.PositionToSurface : XWaylandSurfaceItem.ManualPosition
                z: (waylandSurface && waylandSurface.isActivated) ? 1 : 0

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Math.max(surface.minimumSize.width, 100)
                Layout.minimumHeight: Math.max(surface.minimumSize.height, 50)
                Layout.maximumWidth: surface.maximumSize.width
                Layout.maximumHeight: surface.maximumSize.height
                Component.onCompleted: {
                    if (Layout.horizontalStretchFactor !== undefined) {
                        // introduced in Qt 6.5
                        Layout.horizontalStretchFactor = 1
                    }
                    if (Layout.verticalStretchFactor !== undefined) {
                        Layout.verticalStretchFactor = 1
                    }
                }

                TiledToplevelHelper {
                    id: helper

                    surface: surface
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

// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

Item {
    id: root

    function getXdgSurfaceFromWaylandSurface(surface) {
        let finder = function(props) {
            if (props.waylandSurface === surface)
                return true
        }

        let toplevel = QmlHelper.xdgSurfaceManager.getIf(toplevelComponent, finder)
        if (toplevel) {
            return {
                parent: toplevel,
                xdgSurface: toplevel
            }
        }

        let popup = QmlHelper.xdgSurfaceManager.getIf(popupComponent, finder)
        if (popup) {
            return {
                parent: popup,
                xdgSurface: popup.xdgSurface
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
                //Layout.horizontalStretchFactor: 1
                //Layout.verticalStretchFactor: 1

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
                property var xdgParent: root.getXdgSurfaceFromWaylandSurface(waylandSurface.parentXdgSurface)

                parent: xdgParent ? xdgParent.parent : root
                visible: xdgParent.xdgSurface.effectiveVisible && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
                x: surface.implicitPosition.x + (xdgParent ? xdgParent.xdgSurface.contentItem.x : 0)
                y: surface.implicitPosition.y + (xdgParent ? xdgParent.xdgSurface.contentItem.x : 0)
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
                //Layout.horizontalStretchFactor: 1
                //Layout.verticalStretchFactor: 1

                TiledToplevelHelper {
                    id: helper

                    surface: surface
                    waylandSurface: surface.waylandSurface
                    creator: xwaylandComponent
                }
            }
        }
    }
}

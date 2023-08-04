// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server

Item {
    id: root

    property DynamicCreator creator

    function getXdgSurfaceFromWaylandSurface(surface) {
        let finder = function(props) {
            if (props.waylandSurface === surface)
                return true
        }

        let toplevel = creator.getIf(toplevelComponent, finder)
        if (toplevel) {
            return {
                parent: toplevel,
                xdgSurface: toplevel
            }
        }

        let popup = creator.getIf(popupComponent, finder)
        if (popup) {
            return {
                parent: popup,
                xdgSurface: popup.xdgSurface
            }
        }

        return null
    }

    DynamicCreatorComponent {
        id: toplevelComponent
        creator: root.creator
        chooserRole: "type"
        chooserRoleValue: "toplevel"

        XdgSurface {
            id: surface

            property bool activated: waylandSurface === Helper.activatedSurface
            property OutputPositioner output
            property CoordMapper outputCoordMapper

            visible: waylandSurface && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled

            states: [
                State {
                    name: "maximize"
                    when: waylandSurface && waylandSurface.isMaximized && outputCoordMapper
                    PropertyChanges {
                        restoreEntryValues: true
                        surface {
                            x: outputCoordMapper.x
                            y: outputCoordMapper.y + output.topMargin
                            width: outputCoordMapper.width
                            height: outputCoordMapper.height - output.topMargin
                        }
                    }
                }
            ]

            transitions: Transition {
                NumberAnimation {
                    properties: "x,y,width,height"
                    duration: 100
                }
                onRunningChanged: {
                    if (running && waylandSurface.isMaximized) {
                        surface.resizeMode = SurfaceItem.SizeToSurface
                    } else if (!running && !waylandSurface.isMaximized) {
                        surface.resizeMode = SurfaceItem.SizeFromSurface
                    }
                }
            }

            function getPrimaryOutputPositioner() {
                let output = waylandSurface.surface.primaryOutput
                if (!output)
                    return null
                return output.OutputPositioner.positioner
            }

            function updateOutputCoordMapper() {
                let output = getPrimaryOutputPositioner()
                if (!output)
                    return

                surface.output = output
                surface.outputCoordMapper = surface.CoordMapper.helper.get(output)
            }

            onActivatedChanged: {
                if (activated)
                    WaylibHelper.itemStackToTop(this)
            }

            Connections {
                target: waylandSurface

                function onRequestMove(seat, serial) {
                    if (waylandSurface.isMaximized)
                        return

                    Helper.startMove(waylandSurface, surface, surface.eventItem, seat, serial)
                }

                function onRequestResize(seat, edges, serial) {
                    if (waylandSurface.isMaximized)
                        return

                    Helper.startResize(waylandSurface, surface, surface.eventItem, seat, edges, serial)
                }

                function onResizeingChanged() {
                    if (waylandSurface.isResizeing)
                        surface.resizeMode = SurfaceItem.SizeToSurface
                    else
                        surface.resizeMode = SurfaceItem.SizeFromSurface
                }

                function rectMarginsRemoved(rect, left, top, right, bottom) {
                    rect.x += left
                    rect.y += top
                    rect.width -= (left + right)
                    rect.height -= (top + bottom)
                    return rect
                }

                function onRequestMaximize() {
                    if (waylandSurface.isResizeing)
                        return

                    if (waylandSurface.isMaximized)
                        return

                    surface.updateOutputCoordMapper()
                    waylandSurface.setMaximize(true)
                }

                function onRequestToNormalState() {
                    if (waylandSurface.isResizeing)
                        return

                    if (!waylandSurface.isMaximized)
                        return

                    waylandSurface.setMaximize(false)
                }
            }

            Component.onCompleted: {
                forceActiveFocus()
                Helper.activatedSurface = waylandSurface

                if (waylandSurface.isMaximized) {
                    surface.updateOutputCoordMapper()
                }
            }
        }
    }

    DynamicCreatorComponent {
        id: popupComponent
        creator: root.creator
        chooserRole: "type"
        chooserRoleValue: "popup"

        Popup { // TODO: ensure the cursor item after Popup
            id: popup

            required property WaylandXdgSurface waylandSurface

            property alias xdgSurface: surface
            property var xdgParent: waylandSurface ? root.getXdgSurfaceFromWaylandSurface(waylandSurface.parentXdgSurface) : null

            parent: xdgParent ? xdgParent.parent : root
            visible: waylandSurface && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
            x: surface.implicitPosition.x + (xdgParent ? xdgParent.xdgSurface.contentItem.x : 0)
            y: surface.implicitPosition.y + (xdgParent ? xdgParent.xdgSurface.contentItem.x : 0)
            padding: 0
            background: null

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
}

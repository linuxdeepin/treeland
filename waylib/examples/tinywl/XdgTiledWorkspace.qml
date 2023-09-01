// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

Item {
    id: root

    required property DynamicCreator creator

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

    GridLayout {
        anchors.fill: parent
        columns: Math.floor(root.width / 1920 * 4)

        DynamicCreatorComponent {
            id: toplevelComponent
            creator: root.creator
            chooserRole: "type"
            chooserRoleValue: "toplevel"
            autoDestroy: false

            onObjectRemoved: function (obj) {
                obj.doDestroy()
            }

            XdgSurface {
                id: surface

                property bool activated: waylandSurface === Helper.activatedSurface
                property OutputPositioner output
                property CoordMapper outputCoordMapper
                property bool mapped: waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
                property bool pendingDestroy: false

                resizeMode: SurfaceItem.SizeToSurface
                z: activated ? 1 : 0

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Math.max(minimumSize.width, 100)
                Layout.minimumHeight: Math.max(minimumSize.height, 50)
                Layout.maximumWidth: maximumSize.width
                Layout.maximumHeight: maximumSize.height
                Layout.horizontalStretchFactor: 1
                Layout.verticalStretchFactor: 1

                OpacityAnimator {
                    id: hideAnimation
                    duration: 300
                    target: surface
                    from: 1
                    to: 0

                    onStopped: {
                        surface.visible = false
                        if (pendingDestroy)
                            toplevelComponent.destroyObject(surface)
                    }
                }

                onEffectiveVisibleChanged: {
                    if (surface.effectiveVisible) {
                        // Apply the WSurfaceItem's size to wl_surface
                        surface.resize(SurfaceItem.SizeToSurface)
                        surface.resizeMode = SurfaceItem.SizeToSurface
                    } else {
                        surface.resizeMode = SurfaceItem.ManualResize
                    }
                }

                onMappedChanged: {
                    if (pendingDestroy)
                        return

                    // When Socket is enabled and mapped becomes false, set visible
                    // after hideAnimation complete， Otherwise set visible directly.
                    if (mapped || !waylandSurface.WaylandSocket.rootSocket.enabled) {
                        visible = mapped
                        return
                    }

                    // do animation for window close
                    hideAnimation.start()
                }

                function doDestroy() {
                    pendingDestroy = true

                    if (!visible || !hideAnimation.running) {
                        toplevelComponent.destroyObject(surface)
                        return
                    }

                    // unbind some properties
                    mapped = visible
                    activated = false
                }

                Component.onCompleted: {
                    forceActiveFocus()
                    Helper.activatedSurface = waylandSurface
                }
            }
        }

        DynamicCreatorComponent {
            id: popupComponent
            creator: root.creator
            chooserRole: "type"
            chooserRoleValue: "popup"

            Popup {
                id: popup

                required property WaylandXdgSurface waylandSurface

                property alias xdgSurface: surface
                property var xdgParent: root.getXdgSurfaceFromWaylandSurface(waylandSurface.parentXdgSurface)

                parent: xdgParent ? xdgParent.parent : root
                visible: xdgParent.xdgSurface.effectiveVisible && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
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
}

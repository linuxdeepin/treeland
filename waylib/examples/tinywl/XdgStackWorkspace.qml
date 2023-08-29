// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Tinywl

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

    Item {
        anchors {
            top: parent.top
            left: parent.left
            bottom: parent.bottom
            margins: 8
        }

        width: 250

        ListView {
            id: dock

            model: ListModel {
                id: dockModel

                function removeSurface(surface) {
                    for (var i = 0; i < dockModel.count; i++) {
                        if (dockModel.get(i).source === surface) {
                            dockModel.remove(i);
                            break;
                        }
                    }
                }
            }
            height: Math.min(parent.height, contentHeight)
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                right: parent.right
            }

            spacing: 8

            delegate: ShaderEffectSource {
                id: dockitem
                width: 100; height: 100
                sourceItem: source
                smooth: true

                MouseArea {
                    anchors.fill: parent;
                    onClicked: {
                        dockitem.sourceItem.cancelMinimize();
                    }
                }
            }
        }
    }

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

            property OutputPositioner output
            property CoordMapper outputCoordMapper
            property bool mapped: waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
            property bool pendingDestroy: false
            property var lastResizeMode

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
                    if (!waylandSurface || !surface.effectiveVisible)
                        return

                    if (running && waylandSurface.isMaximized) {
                        surface.resizeMode = SurfaceItem.SizeToSurface
                    } else if (!running && !waylandSurface.isMaximized) {
                        surface.resizeMode = SurfaceItem.SizeFromSurface
                    }
                }
            }

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
                    console.assert(lastResizeMode !== undefined,
                                   "Can't restore the resize mode on effective visible changed")

                    // Apply the WSurfaceItem's size to wl_surface
                    surface.resize(SurfaceItem.SizeToSurface)
                    surface.resizeMode = lastResizeMode

                    if (waylandSurface && waylandSurface.isActivated)
                        surface.forceActiveFocus()
                } else {
                    Helper.cancelMoveResize(surface)
                    lastResizeMode = surface.resizeMode
                    surface.resizeMode = SurfaceItem.ManualResize
                }
            }

            onMappedChanged: {
                if (pendingDestroy)
                    return

                // When Socket is enabled and mapped becomes false, set visible
                // after hideAnimation complete， Otherwise set visible directly.
                if (mapped) {
                    if (waylandSurface.isMinimized) {
                        visible = false;
                        dockModel.append({ source: surface });
                    } else {
                        visible = true;
                    }
                } else { // if not mapped
                    if (waylandSurface.isMinimized) {
                        // mapped becomes false but not pendingDestroy
                        dockModel.removeSurface(surface)
                    }

                    if (!waylandSurface.WaylandSocket.rootSocket.enabled) {
                        visible = false;
                    } else {
                        // do animation for window close
                        hideAnimation.start()
                    }
                }
            }

            function doDestroy() {
                pendingDestroy = true

                if (!visible || !hideAnimation.running) {
                    if (waylandSurface.isMinimized) {
                        // mapped becomes false and pendingDestroy
                        dockModel.removeSurface(surface)
                    }

                    toplevelComponent.destroyObject(surface)
                    return
                }

                // unbind some properties
                mapped = visible
                activated = false
                states = null
                transitions = null
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

            function cancelMinimize () {
                if (waylandSurface.isResizeing)
                    return

                if (!waylandSurface.isMinimized)
                    return

                Helper.activatedSurface = waylandSurface

                visible = true;

                dockModel.removeSurface(surface)
                waylandSurface.setMinimize(false)
            }

            Connections {
                target: waylandSurface

                function onActivateChanged() {
                    if (waylandSurface.isActivated) {
                        WaylibHelper.itemStackToTop(surface)
                        if (surface.effectiveVisible)
                            surface.forceActiveFocus()
                    } else {
                        surface.focus = false
                    }
                }

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
                    else if (surface.resizeMode === SurfaceItem.SizeToSurface)
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

                function onRequestCancelMaximize() {
                    if (waylandSurface.isResizeing)
                        return

                    if (!waylandSurface.isMaximized)
                        return

                    waylandSurface.setMaximize(false)
                }

                function onRequestMinimize() {
                    if (waylandSurface.isResizeing)
                        return

                    if (waylandSurface.isMinimized)
                        return

                    focus = false;
                    if (Helper.activeSurface === surface)
                        Helper.activeSurface = null;

                    visible = false;
                    dockModel.append({ source: surface });
                    waylandSurface.setMinimize(true)
                }

                function onRequestCancelMinimize() {
                    waylandSurface.cancelMinimize();
                }
            }

            Component.onCompleted: {
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
}

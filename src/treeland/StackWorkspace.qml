// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import TreeLand

Item {
    id: root

    function getSurfaceItemFromWaylandSurface(surface) {
        let finder = function(props) {
            if (props.waylandSurface === surface)
                return true
        }

        let toplevel = QmlHelper.xdgSurfaceManager.getIf(toplevelComponent, finder)
        if (toplevel) {
            return {
                shell: toplevel,
                item: toplevel
            }
        }

        let popup = QmlHelper.xdgSurfaceManager.getIf(popupComponent, finder)
        if (popup) {
            return {
                shell: popup,
                item: popup.xdgSurface
            }
        }

        let xwayland = QmlHelper.xwaylandSurfaceManager.getIf(xwaylandComponent, finder)
        if (xwayland) {
            return {
                shell: xwayland,
                item: xwayland
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
        creator: QmlHelper.xdgSurfaceManager
        chooserRole: "type"
        chooserRoleValue: "toplevel"
        autoDestroy: false

        onObjectRemoved: function (obj) {
            obj.doDestroy()
        }

        // TODO: Support server decoration
        XdgSurface {
            id: surface

            property var doDestroy: helper.doDestroy
            property var cancelMinimize: helper.cancelMinimize

            StackToplevelHelper {
                id: helper
                surface: surface
                waylandSurface: surface.waylandSurface
                dockModel: dock.model
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
            property var xdgParent: root.getSurfaceItemFromWaylandSurface(waylandSurface.parentXdgSurface)

            parent: xdgParent ? xdgParent.shell : root
            visible: xdgParent.item.effectiveVisible && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
            x: {
                if (!xdgParent)
                    return surface.implicitPosition.x
                return surface.implicitPosition.x / xdgParent.item.surfaceSizeRatio + xdgParent.item.contentItem.x
            }
            y: {
                if (!xdgParent)
                    return surface.implicitPosition.y
                return surface.implicitPosition.y / xdgParent.item.surfaceSizeRatio + xdgParent.item.contentItem.y
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
            property var cancelMinimize: helper.cancelMinimize
            property var surfaceParent: root.getSurfaceItemFromWaylandSurface(waylandSurface.parentXWaylandSurface)

            surface: waylandSurface
            parentSurfaceItem: surfaceParent ? surfaceParent.item : null
            z: waylandSurface.bypassManager ? 1 : 0 // TODO: make to enum type
            positionMode: {
                if (!surface.effectiveVisible)
                    return XWaylandSurfaceItem.ManualPosition

                return (TreeLandHelper.movingItem === surface || resizeMode === SurfaceItem.SizeToSurface)
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
                if (surface.effectiveVisible)
                    surface.move(XWaylandSurfaceItem.PositionToSurface)
            }

            // TODO: ensure the event to WindowDecoration before WSurfaceItem::eventItem on surface's edges
            // maybe can use the SinglePointHandler?
            WindowDecoration {
                id: decoration

                property bool enable: !waylandSurface.bypassManager
                                      && waylandSurface.decorationsType !== XWaylandSurface.DecorationsNoBorder

                anchors.fill: parent
                z: surface.contentItem.z - 1
                visible: enable
            }

            StackToplevelHelper {
                id: helper
                surface: surface
                waylandSurface: surface.waylandSurface
                dockModel: dockModel
                creator: xwaylandComponent
                decoration: decoration
            }

            OutputLayoutItem {
                anchors.fill: parent
                layout: QmlHelper.layout

                onEnterOutput: function(output) {
                    if (surface.waylandSurface.surface)
                        surface.waylandSurface.surface.enterOutput(output);
                }
                onLeaveOutput: function(output) {
                    if (surface.waylandSurface.surface)
                        surface.waylandSurface.surface.leaveOutput(output);
                }
            }
        }
    }
}

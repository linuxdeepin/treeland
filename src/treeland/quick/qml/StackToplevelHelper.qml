// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import QtQuick.Particles
import TreeLand
import TreeLand.Protocols
import TreeLand.Utils
import TreeLand.Protocols

Item {
    id: root

    required property SurfaceItem surface
    required property ToplevelSurface waylandSurface
    required property ListModel dockModel
    required property ListModel switcherModel
    required property DynamicCreatorComponent creator
    property WindowDecoration decoration
    property var quickForeignToplevelManageMapper: waylandSurface.TreeLandForeignToplevelManagerV1

    property OutputItem output
    property CoordMapper outputCoordMapper
    property bool mapped: waylandSurface.surface && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
    property bool pendingDestroy: false
    property bool isMaximize: waylandSurface && waylandSurface.isMaximized && outputCoordMapper

    Binding {
        target: surface
        property: "states"
        restoreMode: Binding.RestoreNone
        value: State {
            name: "maximize"
            when: isMaximize
            PropertyChanges {
                restoreEntryValues: true
                target: root.surface

                x: outputCoordMapper.x + TreeLandHelper.getLeftExclusiveMargin(waylandSurface)
                y: outputCoordMapper.y + output.topMargin + TreeLandHelper.getTopExclusiveMargin(waylandSurface)
                width: outputCoordMapper.width - TreeLandHelper.getLeftExclusiveMargin(waylandSurface) - TreeLandHelper.getRightExclusiveMargin(waylandSurface)
                height: outputCoordMapper.height - output.topMargin - TreeLandHelper.getTopExclusiveMargin(waylandSurface) - TreeLandHelper.getBottomExclusiveMargin(waylandSurface)
            }
        }
    }

    Binding {
        target: surface
        property: "transitions"
        restoreMode: Binding.RestoreNone
        value: Transition {
            id: stateTransition

            NumberAnimation {
                properties: "x,y,width,height"
                duration: 100
            }
        }
    }

    Binding {
        target: surface
        property: "resizeMode"
        value: {
            if (!surface.effectiveVisible)
                return SurfaceItem.ManualResize
            if (TreeLandHelper.resizingItem === surface
                    || stateTransition.running
                    || waylandSurface.isMaximized)
                return SurfaceItem.SizeToSurface
            return SurfaceItem.SizeFromSurface
        }
        restoreMode: Binding.RestoreNone
    }

    Loader {
        id: closeAnimation
    }

    Component {
        id: closeAnimationComponent

        CloseAnimation {
            onStopped: {
                if (pendingDestroy)
                    creator.destroyObject(surface)
            }
        }
    }

    Connections {
        target: surface

        function onEffectiveVisibleChanged() {
            if (surface.effectiveVisible) {
                console.assert(surface.resizeMode !== SurfaceItem.ManualResize,
                               "The surface's resizeMode Shouldn't is ManualResize")
                // Apply the WSurfaceItem's size to wl_surface
                surface.resize(SurfaceItem.SizeToSurface)

                if (waylandSurface && waylandSurface.isActivated)
                    surface.forceActiveFocus()
            } else {
                TreeLandHelper.cancelMoveResize(surface)
            }
        }
    }

    Connections {
        target: quickForeignToplevelManageMapper

        function onRequestMaximize(maximized) {
            if (maximized) {
                connOfSurface.onRequestCancelMaximize()
            } else {
                connOfSurface.onRequestMaximize()
            }
        }

        function onRequestMinimize(minimized) {
            if (minimized) {
                connOfSurface.onRequestMinimize()
                TreeLandHelper.activatedSurface = null
            } else {
                connOfSurface.onRequestCancelMinimize()
            }
        }

        function onRequestActivate(activated) {
            if (activated && waylandSurface.isMinimized) {
                cancelMinimize()
            }

            surface.focus = activated
            TreeLandHelper.activatedSurface = activated ? surface : null
        }

        function onRequestFullscreen(fullscreen) {
            // TODO: add full screen action
        }

        function onRequestClose() {
            if (waylandSurface.close)
                waylandSurface.close()
            else
                waylandSurface.surface.unmap()
        }

        function onRectangleChanged(edges) {
            connOfSurface.onRequestResize(null, edges, null)
        }
    }

    Connections {
        target: decoration

        // TODO: Don't call connOfSurface

        function onRequestMove() {
            connOfSurface.onRequestMove(null, 0)
        }

        function onRequestResize(edges) {
            connOfSurface.onRequestResize(null, edges, null)
        }

        function onRequestMinimize() {
            connOfSurface.onRequestMinimize()
        }

        function onRequestToggleMaximize(max) {
            if (max) {
                connOfSurface.onRequestMaximize()
            } else {
                connOfSurface.onRequestCancelMaximize()
            }
        }

        function onRequestClose() {
            if (waylandSurface.close) // WXWaylandSurface
                waylandSurface.close()
            else
                waylandSurface.surface.unmap()
        }
    }

    onMappedChanged: {
        console.log("onMappedChanged!", TreeLandHelper.clientName(waylandSurface.surface))

        if (TreeLandHelper.clientName(waylandSurface.surface) === "dofi") {
            z = 3
            surface.anchors.centerIn = workspace
        }

        // When Socket is enabled and mapped becomes false, set visible
        // after closeAnimation complete， Otherwise set visible directly.
        if (mapped) {
            if (waylandSurface.isMinimized) {
                surface.visible = false;
                dockModel.append({ source: surface });
            } else {
                surface.visible = true;

                if (surface.effectiveVisible)
                    TreeLandHelper.activatedSurface = waylandSurface
            }

            switcherModel.append({ source: surface });
        } else { // if not mapped
            if (waylandSurface.isMinimized) {
                // mapped becomes false but not pendingDestroy
                dockModel.removeSurface(surface)
            }

            if (!waylandSurface.WaylandSocket.rootSocket.enabled) {
                surface.visible = false;
            } else {
                // do animation for window close
                closeAnimation.parent = surface.parent
                closeAnimation.anchors.fill = surface
                closeAnimation.sourceComponent = closeAnimationComponent
                closeAnimation.item.start(surface)
            }
            switcherModel.removeSurface(surface)
        }
    }

    function doDestroy() {
        pendingDestroy = true

        switcherModel.removeSurface(surface)

        if (!surface.visible || !closeAnimation.active) {
            if (waylandSurface.isMinimized) {
                // mapped becomes false and pendingDestroy
                dockModel.removeSurface(surface)
            }

            creator.destroyObject(surface)
            return
        }

        // unbind some properties
        mapped = false
        surface.states = null
        surface.transitions = null
    }

    function getPrimaryOutputItem() {
        let output = waylandSurface.surface.primaryOutput
        if (!output)
            return null
        return output.OutputItem.item
    }

    function updateOutputCoordMapper() {
        let output = getPrimaryOutputItem()
        if (!output)
            return

        root.output = output
        root.outputCoordMapper = surface.CoordMapper.helper.get(output)
    }

    function cancelMinimize () {
        if (waylandSurface.isResizeing)
            return

        if (!waylandSurface.isMinimized)
            return

        TreeLandHelper.activatedSurface = waylandSurface

        surface.visible = true;

        dockModel.removeSurface(surface)
        waylandSurface.setMinimize(false)
    }

    Connections {
        id: connOfSurface

        target: waylandSurface
        ignoreUnknownSignals: true

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

            if (!surface.effectiveVisible)
                return

            TreeLandHelper.startMove(waylandSurface, surface, seat, serial)
        }

        function onRequestResize(seat, edges, serial) {
            if (waylandSurface.isMaximized)
                return

            if (!surface.effectiveVisible)
                return

            TreeLandHelper.startResize(waylandSurface, surface, seat, edges, serial)
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

            if (!surface.effectiveVisible)
                return

            updateOutputCoordMapper()
            waylandSurface.setMaximize(true)
        }

        function onRequestCancelMaximize() {
            if (waylandSurface.isResizeing)
                return

            if (!waylandSurface.isMaximized)
                return

            if (!surface.effectiveVisible)
                return

            waylandSurface.setMaximize(false)
        }

        function onRequestMinimize() {
            if (waylandSurface.isResizeing)
                return

            if (waylandSurface.isMinimized)
                return

            if (!surface.effectiveVisible)
                return

            surface.focus = false;
            if (TreeLandHelper.activatedSurface === surface)
                TreeLandHelper.activatedSurface = null;

            surface.visible = false;
            dockModel.append({ source: surface });
            waylandSurface.setMinimize(true)
        }

        function onRequestCancelMinimize() {
            if (!surface.effectiveVisible)
                return

            cancelMinimize();
        }
    }

    Component.onCompleted: {
        if (waylandSurface.isMaximized) {
            updateOutputCoordMapper()
        }
    }
}

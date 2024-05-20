// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import QtQuick.Particles
import TreeLand
import TreeLand.Protocols
import TreeLand.Utils

Item {
    id: root

    required property SurfaceItem surface
    required property ToplevelSurface waylandSurface
    required property DynamicCreatorComponent creator
    property WindowDecoration decoration
    property var quickForeignToplevelManageMapper: waylandSurface.TreeLandForeignToplevelManagerV1

    property OutputItem output
    property CoordMapper outputCoordMapper
    property bool mapped: waylandSurface.surface && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
    property bool pendingDestroy: false
    property bool isMaximize: waylandSurface && waylandSurface.isMaximized && outputCoordMapper
    property bool isFullScreen: waylandSurface && waylandSurface.isFullScreen && outputCoordMapper
    property bool showCloseAnimation: false
    property bool showNewAnimation: true

    // For Maximize
    function getMaximizeX() {
        return outputCoordMapper.x + Helper.getLeftExclusiveMargin(waylandSurface)
    }
    function getMaximizeY() {
        return outputCoordMapper.y + Helper.getTopExclusiveMargin(waylandSurface)
    }
    function getMaximizeWidth() {
        return outputCoordMapper.width - Helper.getLeftExclusiveMargin(waylandSurface) - Helper.getRightExclusiveMargin(waylandSurface)
    }
    function getMaximizeHeight() {
        return outputCoordMapper.height - Helper.getTopExclusiveMargin(waylandSurface) - Helper.getBottomExclusiveMargin(waylandSurface)
    }

    // For Fullscreen
    function getFullscreenX() {
        return outputCoordMapper.x
    }
    function getFullscreenY() {
        return outputCoordMapper.y
    }
    function getFullscreenWidth() {
        return outputCoordMapper.width
    }
    function getFullscreenHeight() {
        return outputCoordMapper.height
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
            if ( stateTransition.running
                    || waylandSurface.isMaximized)
                return SurfaceItem.SizeToSurface
            return SurfaceItem.SizeFromSurface
        }
        restoreMode: Binding.RestoreNone
    }
    // if surface mapped when not visible, it will change mode to sizefromsurf
    // but mode change is not applied if no resize event happens afterwards, so trigger resize here
    Connections {
        target: surface
        function onResizeModeChanged() {
            if (surface.resizeMode != SurfaceItem.ManualResize)
                surface.resize(surface.resizeMode)
        }
    }

    Loader {
        id: newAnimation
    }

    Component {
        id: newAnimationComponent

        NewAnimation {
            target: surface
        }
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
            } else {
                Helper.cancelMoveResize(surface)
            }
        }
    }

    Repeater {
        model: [root.quickForeignToplevelManager || null,root.decoration]

        Item {
            Connections {
                target: modelData
                ignoreUnknownSignals: true

                function onRequestMaximize(maximized) {
                    if (maximized) {
                        doMaximize()
                    } else {
                        cancelMaximize()
                    }
                }

                function onRequestMinimize(minimized = true) {
                    if (minimized) {
                        doMinimize()
                    } else {
                        cancelMinimize()
                    }
                }

                function onRequestActivate(activated) {
                    if (activated && waylandSurface.isMinimized) {
                        cancelMinimize()
                    }

                    surface.focus = activated
                    if (activated)
                        Helper.activatedSurface = waylandSurface
                }

                function onRequestFullscreen(fullscreen) {
                    if (fullscreen) {
                        doFullscreen();
                    } else {
                        cancelFullscreen();
                    }
                }

                function onRequestClose() {
                    //quickForeignToplevelManageMapper.onRequestActivate(false) // TODO: @rewine
                    Helper.closeSurface(waylandSurface.surface);
                }

                function onRectangleChanged(edges) {
                    // error
                    doResize(null, edges, null)
                }

                function onRequestMove() {
                    doMove(null, 0)
                }

                function onRequestResize(edges) {
                    doResize(null, edges, null)
                }
            }
        }
    }

    function workspace() { return QmlHelper.workspaceManager.workspacesById.get(surface.workspaceId) }

    onMappedChanged: {
        console.log("onMappedChanged!", Helper.clientName(waylandSurface.surface))

        // When Socket is enabled and mapped becomes false, set visible
        // after closeAnimation complete， Otherwise set visible directly.
        if (mapped) {
            if (waylandSurface.isMinimized) {
                surface.visible = false;
            } else {
                surface.visible = true;

                if (surface.parent.isCurrentWorkspace)
                    Helper.activatedSurface = waylandSurface

                if (showNewAnimation) {
                    newAnimation.parent = surface.parent
                    newAnimation.anchors.fill = surface
                    newAnimation.sourceComponent = newAnimationComponent
                    newAnimation.item.start()
                }
            }
            workspace().surfaces.appendEnhanced({item: surface})
            QmlHelper.workspaceManager.allSurfaces.append({item: surface})
        } else { // if not mapped
            if (!waylandSurface.WaylandSocket.rootSocket.enabled) {
                surface.visible = false;
            } else {
                // if don't show CloseAnimation will destroyObject in doDestroy, here is too early
                if (showCloseAnimation) {
                    // do animation for window close
                    closeAnimation.parent = surface.parent
                    closeAnimation.anchors.fill = surface
                    closeAnimation.sourceComponent = closeAnimationComponent
                    closeAnimation.item.start(surface)
                }
            }
            workspace().surfaces.removeIf((val) => val.item === surface)
            QmlHelper.workspaceManager.allSurfaces.removeIf((val) => val.item === surface)
            if (Helper.activatedSurface === waylandSurface)
                surface.parent.selectSurfaceToActivate()
        }
    }

    function doDestroy() {
        pendingDestroy = true
        // may have been removed when unmapped?
        workspace().surfaces.removeIf((val) => val.item === surface)
        QmlHelper.workspaceManager.allSurfaces.removeIf((val) => val.item === surface)

        if (!surface.visible || !closeAnimation.active) {
            creator.destroyObject(surface)
            return
        }

        // unbind some properties
        mapped = false
        surface.states = null
        surface.transitions = null

        if (!showCloseAnimation) {
            creator.destroyObject(surface)
        }
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

    Connections {
        id: connOfSurface

        target: waylandSurface
        ignoreUnknownSignals: true

        function onActivateChanged() {
            if (waylandSurface.isActivated) {
                WaylibHelper.itemStackToTop(surface)
                if (surface.effectiveVisible) {
                    Helper.activatedSurface = waylandSurface
                }
            }
        }

        function onRequestMove(seat, serial) {
            doMove(seat, serial)
        }

        function onRequestResize(seat, edges, serial) {
            doResize(seat, edges, serial)
        }

        function onRequestMaximize() {
            doMaximize()
        }

        function onRequestCancelMaximize() {
            cancelMaximize()
        }

        function onRequestMinimize() {
            doMinimize()
        }

        function onRequestCancelMinimize() {
            cancelMinimize()
        }

        function onRequestFullscreen() {
            doFullscreen()
        }

        function onRequestCancelFullscreen() {
            cancelFullscreen()
        }
    }

    function doMove(seat, serial) {
        if (waylandSurface.isMaximized)
            return

        Helper.startMove(waylandSurface, surface, seat, serial)
    }

    function doResize(seat, edges, serial) {
        if (waylandSurface.isMaximized)
            return

        Helper.startResize(waylandSurface, surface, seat, edges, serial)
    }

    function doMaximize() {
        if (waylandSurface.isResizeing)
            return

        updateOutputCoordMapper()
        waylandSurface.setMaximize(true)
    }

    function cancelMaximize() {
        if (waylandSurface.isResizeing)
            return

        waylandSurface.setMaximize(false)
    }

    function doMinimize() {
        if (waylandSurface.isResizeing)
            return

        if (waylandSurface.isMinimized)
            return

        surface.focus = false;
        surface.visible = false;
        waylandSurface.setMinimize(true)

        if (Helper.activatedSurface === waylandSurface) {
            surface.parent.selectSurfaceToActivate()
        }
    }

    function cancelMinimize () {
        if (waylandSurface.isResizeing)
            return

        if (!waylandSurface.isMinimized)
            return

        Helper.activatedSurface = waylandSurface

        surface.visible = true;

        waylandSurface.setMinimize(false)
    }

    function doFullscreen() {
        if (waylandSurface.isResizeing)
            return

        updateOutputCoordMapper()
        waylandSurface.setFullScreen(true)
    }

    function cancelFullscreen() {
        if (waylandSurface.isResizeing)
            return

        waylandSurface.setFullScreen(false)
    }

    Component.onCompleted: {
        if (waylandSurface.isMaximized) {
            updateOutputCoordMapper()
        }
    }

    // for workspace management
    Connections {
        target: surface
        property ToplevelContainer parentCached: { parentCached = target.parent }
        function onWorkspaceIdChanged() {
            // sync state to succesive models
            console.log('workspaceIdChanged, reparenting to id=',workspaceId)
        }
        function onParentChanged() {
            parentCached?.surfaces.removeIf((val) => val.item === surface)
            parentCached = target.parent
            target.parent.surfaces.appendEnhanced({item: surface})
        }
    }
}

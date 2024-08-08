// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import QtQuick.Particles
import QtQuick.Effects
import TreeLand
import TreeLand.Utils
import TreeLand.Protocols

FocusScope {
    property bool anchorWidth: false
    property bool anchorHeight: false
    // the output attached to, default is primary
    required property OutputItem activeOutputItem
    property OutputItem output: getInitialOutputItem()
    property CoordMapper outputCoordMapper: output ? this.CoordMapper.helper.get(output) : null
    property bool mapped: wSurface.surface && wSurface.surface.mapped && wSurface.WaylandSocket.rootSocket.enabled
    property bool pendingDestroy: false

    property alias surfaceItem: surfaceItem
    property var personalizationMapper: PersonalizationV1.Attached(wSurface)

    required property DynamicCreatorComponent creatorCompoment
    required property WaylandLayerSurface wSurface
    required property string type

    // TODO: should move to protocol?
    readonly property bool forceBlur: {
        return Helper.shouldForceBlur(wSurface)
    }
    readonly property bool forceBackground: {
        return Helper.isLaunchpad(wSurface)
    }

    id: root
    z: zValueFormLayer(wSurface.layer)

    LayerSurfaceItem {
        anchors.centerIn: parent
        focus: true
        shellSurface: wSurface

        id: surfaceItem

        onHeightChanged: {
            if (!anchorHeight)
                parent.height = height
        }

        onWidthChanged: {
            if (!anchorWidth)
                parent.width = width
        }

        onEffectiveVisibleChanged: {
            if (effectiveVisible && surface.isActivated)
                forceActiveFocus()
        }
    }

    OutputLayoutItem {
        anchors.fill: parent
        layout: Helper.outputLayout

        onEnterOutput: function(output) {
            Helper.registerExclusiveZone(wSurface)
            wSurface.surface.enterOutput(output)
            Helper.onSurfaceEnterOutput(wSurface, surfaceItem, output)
        }
        onLeaveOutput: function(output) {
            Helper.unregisterExclusiveZone(wSurface)
            wSurface.surface.leaveOutput(output)
            Helper.onSurfaceLeaveOutput(wSurface, surfaceItem, output)
        }
    }

    Loader {
        id: animation
        active: false
        parent: surfaceItem.parent
        anchors.fill: surfaceItem
    }

    Component {
        id: windowAnimation

        LayerShellAnimation {
            target: surfaceItem
            direction: mapped ? LayerShellAnimation.Direction.Show : LayerShellAnimation.Direction.Hide
            position: wSurface.getExclusiveZoneEdge()
            enableBlur: personalizationMapper.backgroundType === Personalization.Blend || forceBlur
            onStopped: {
                if (!mapped) {
                    if (pendingDestroy) {
                        creatorCompoment.destroyObject(root)
                    } else {
                        surfaceItem.visible = false
                    }
                }
                animation.active = false
            }
        }
    }

    Component {
        id: launchpadAnimation

        LaunchpadAnimation {
            target: surfaceItem
            direction: mapped ? LaunchpadAnimation.Direction.Show : LaunchpadAnimation.Direction.Hide
            position: wSurface.getExclusiveZoneEdge()
            onStopped: {
                if (!mapped) {
                    if (pendingDestroy) {
                        creatorCompoment.destroyObject(root)
                    } else {
                        surfaceItem.visible = false
                    }
                }
                animation.active = false
            }
        }
    }

    WallpaperController {
        id: wallpaperController
    }

    Loader {
        active: personalizationMapper.backgroundType === Personalization.Blend || forceBlur
        parent: surfaceItem
        z: surfaceItem.z - 1
        anchors.fill: parent
        sourceComponent: RenderBufferBlitter {
            id: blitter
            anchors.fill: parent
            MultiEffect {
                id: blur

                anchors.fill: parent
                source: blitter.content
                autoPaddingEnabled: false
                blurEnabled: true
                blur: 1.0
                blurMax: 64
                saturation: 0.2
            }
        }
    }

    Rectangle {
        id: cover
        visible: forceBackground && !animation.active
        parent: surfaceItem
        z: surfaceItem.z - 2
        anchors.fill: surfaceItem
        color: 'black'
        opacity: forceBackground && animation.active ? 0 : 0.6
    }

    Loader {
        active: forceBackground
        z: surfaceItem.z - 1
        parent: surfaceItem.parent
        anchors.fill: surfaceItem
        sourceComponent: ShaderEffectSource {
            sourceItem: wallpaperController.proxy
            hideSource: false
            live: true
        }
        opacity: mapped ? 1 : 0
        Behavior on opacity {
            PropertyAnimation {
                duration: 400
                easing.type: Easing.OutExpo
            }
        }
    }

    onMappedChanged: {
        // When Socket is enabled and mapped becomes false, set visible
        // after closeAnimation complete， Otherwise set visible directly.
        if (mapped) {
            Helper.registerExclusiveZone(wSurface)
            refreshMargin()
            surfaceItem.visible = true
            if (surfaceItem.effectiveVisible)
                Helper.activatedSurface = wSurface

                wallpaperController.output = output.output
                wallpaperController.type = Helper.Scale
        } else { // if not mapped
            Helper.unregisterExclusiveZone(wSurface)
            if (!wSurface.WaylandSocket.rootSocket.enabled) {
                surfaceItem.visible = false
            } else {
                wallpaperController.output = output.output
                wallpaperController.type = Helper.Normal
            }
        }

        if (!animation.active) {
            animation.active = true
            if (forceBackground) {
                animation.sourceComponent = launchpadAnimation
            } else {
                animation.sourceComponent = windowAnimation
            }
        }

        animation.item.start()
    }

    function doDestroy() {
        pendingDestroy = true

       if (!surfaceItem.visible || !animation.active) {
           creatorCompoment.destroyObject(root)
           return
       }

        // unbind some properties
        mapped = false
    }

    function getInitialOutputItem() {
        const output = wSurface.output
        if (!output)
            return activeOutputItem
        return output.OutputItem.item
    }

    function zValueFormLayer(layer) {
        switch (layer) {
        case WaylandLayerSurface.LayerType.Background:
            return -100
        case WaylandLayerSurface.LayerType.Bottom:
            return -50
        case WaylandLayerSurface.LayerType.Top:
            if (wSurface.exclusiveZone > 0) {
                return 51
            }
            return 50
        case WaylandLayerSurface.LayerType.Overlay:
            return 100
        }
        // Should not be reachable
        return -50;
    }

    function refreshAnchors() {
        var top = wSurface.ancher & WaylandLayerSurface.AnchorType.Top
        var bottom = wSurface.ancher & WaylandLayerSurface.AnchorType.Bottom
        var left = wSurface.ancher & WaylandLayerSurface.AnchorType.Left
        var right = wSurface.ancher & WaylandLayerSurface.AnchorType.Right

        anchorWidth = left && right
        anchorHeight = top && bottom

        if (root.outputCoordMapper) {
            anchors.top = top ? root.outputCoordMapper.top : undefined
            anchors.bottom = bottom ? root.outputCoordMapper.bottom : undefined
            anchors.verticalCenter = (top || bottom) ? undefined : root.outputCoordMapper.verticalCenter;
            anchors.left = left ? root.outputCoordMapper.left : undefined
            anchors.right = right ? root.outputCoordMapper.right : undefined
            anchors.horizontalCenter = (left || right) ? undefined : root.outputCoordMapper.horizontalCenter;
        } else {
            console.warn('No outputCoordMapper', root.output)
        }
        // Setting anchors may change the container size which should keep same with surfaceItem
        if (!anchorWidth)
            width = surfaceItem.width
        if (!anchorHeight)
            height = surfaceItem.height
        // Anchors also influence Edge of Exclusive Zone
        if (wSurface.exclusiveZone > 0)
            refreshExclusiveZone()
    }

    function refreshExclusiveZone() {
        Helper.unregisterExclusiveZone(wSurface)
        Helper.registerExclusiveZone(wSurface)
    }

    function refreshMargin() {
        var accpectExclusive = wSurface.exclusiveZone >= 0 ? 1 : 0;
        var exclusiveMargin = Helper.getExclusiveMargins(wSurface)

        var topMargin = wSurface.topMargin + accpectExclusive * exclusiveMargin.top;
        var bottomMargin = wSurface.bottomMargin + accpectExclusive * exclusiveMargin.bottom;
        var leftMargin = wSurface.leftMargin + accpectExclusive * exclusiveMargin.left;
        var rightMargin = wSurface.rightMargin + accpectExclusive * exclusiveMargin.right;
        anchors.topMargin = topMargin;
        anchors.bottomMargin = bottomMargin;
        anchors.leftMargin = leftMargin;
        anchors.rightMargin = rightMargin;
    }

    function configureSurfaceSize() {
        var surfaceWidth = wSurface.desiredSize.width
        var surfaceHeight = wSurface.desiredSize.height

        if (surfaceWidth === 0)
            surfaceWidth = width
        if (surfaceHeight === 0)
            surfaceHeight = height

        if (surfaceWidth && surfaceHeight)
            wSurface.configureSize(Qt.size(surfaceWidth, surfaceHeight))

    }

    onHeightChanged: {
        if (wSurface.desiredSize.height === 0 && height !== 0) {
            configureSurfaceSize()
        }
    }

    onWidthChanged: {
        if (wSurface.desiredSize.width === 0 && width !== 0) {
            configureSurfaceSize()
        }
    }

    Component.onCompleted: {
        if (!root.outputCoordMapper) {
            console.warn('No outputCoordMapper', root.output)
            wSurface.closed()
            return
        }
        refreshAnchors()
        refreshMargin()
        configureSurfaceSize()
    }

    onOutputCoordMapperChanged: {
        refreshAnchors()
    }

    Connections {
        target: wSurface

        function onLayerPropertiesChanged() {
            Helper.unregisterExclusiveZone(wSurface)
            Helper.registerExclusiveZone(wSurface)
            refreshAnchors()
            refreshMargin()
            configureSurfaceSize()
        }

        function onActivateChanged() {
            if (wSurface.isActivated && surfaceItem.effectiveVisible) {
                surfaceItem.forceActiveFocus()
            }
        }
    }

    Connections {
        target: Helper

        function onTopExclusiveMarginChanged() {
            refreshMargin()
        }

        function onBottomExclusiveMarginChanged() {
            refreshMargin()
        }

        function onLeftExclusiveMarginChanged() {
            refreshMargin()
        }

        function onRightExclusiveMarginChanged() {
            refreshMargin()
        }

        function onLockScreenChanged() {
            mapped = !Helper.lockScreen
        }
    }
}

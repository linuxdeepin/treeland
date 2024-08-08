// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Waylib.Server
import TreeLand
import TreeLand.Protocols
import TreeLand.Utils
import org.deepin.dtk 1.0 as D

FocusScope {
    id: root
    function getSurfaceItemFromWaylandSurface(surface) {
        let finder = function(props) {
            if (!props.wSurface)
                return false
            // surface is WToplevelSurface or WSurfce
            if (props.wSurface === surface || props.wSurface.surface === surface)
                return true
        }

        let toplevel = Helper.surfaceCreator.getIf(toplevelComponent, finder)
        if (toplevel) {
            return toplevel
        }

        let popup = Helper.surfaceCreator.getIf(popupComponent, finder)
        if (popup) {
            return popup
        }

        let layer = Helper.surfaceCreator.getIf(layerComponent, finder)
        if (layer) {
            return layer
        }

        let xwayland = Helper.surfaceCreator.getIf(xwaylandComponent, finder)
        if (xwayland) {
            return xwayland
        }

        return null
    }

    function unloadMask() {
        if (switcher.enableAnimation) {
            mask.loaderStatus = 1
            mask.loaderStatus = 0
        } else {
            mask.sourceComponent = undefined
        }
    }

    required property OutputDelegate activeOutputDelegate
    readonly property real hideOpacity: 0

    property var workspaceManager: QmlHelper.workspaceManager
    property int currentWorkspaceId: Helper.currentWorkspaceId

    Connections {
        target: workspaceManager.layoutOrder
        function onCountChanged() {
            if (currentWorkspaceId >= target.count) {
                Helper.currentWorkspaceId = target.count - 1
            }
        }
    }

    // activated workspace driven by surface activation
    property Item activatedSurfaceItem:
        getSurfaceItemFromWaylandSurface(Helper.activatedSurface)?.surfaceItem || null // cannot assign [undefined] to QQuickItem*, need to assign null
    onActivatedSurfaceItemChanged: {
        if (activatedSurfaceItem?.parent?.workspaceRelativeId !== undefined)
            Helper.currentWorkspaceId = activatedSurfaceItem.parent.workspaceRelativeId
        if (Helper.activatedSurface) {
            QmlHelper.winposManager.updateSeq(Helper.activatedSurface.appId, activatedSurfaceItem)
        }
    }

    // activated surface driven by workspace change
    onCurrentWorkspaceIdChanged:
        if (activatedSurfaceItem?.parent?.workspaceRelativeId !== currentWorkspaceId)
            workspaceManager.workspacesById.get(workspaceManager.layoutOrder.get(currentWorkspaceId).wsid)?.selectSurfaceToActivate()

    FocusScope {
        anchors.fill: parent
        visible: !WindowManagementV1.desktopState
        enabled: !switcher.visible && !multitaskView.active
        focus: enabled
        opacity: dockPreview.previewing ? hideOpacity : 1
        z: 0

        Behavior on opacity {
            enabled: !switcher.visible
            NumberAnimation { duration: 300 }
        }

        Repeater {
            model: workspaceManager.layoutOrder
            anchors.fill: parent
            delegate: ToplevelContainer {
                id: container
                objectName: `ToplevelContainer ${wsid}`
                required property int index
                required property int wsid
                property bool isCurrentWorkspace: workspaceRelativeId === currentWorkspaceId
                workspaceId: wsid
                workspaceRelativeId: index
                visible: isCurrentWorkspace && !workspaceAnimation.active
                focus: isCurrentWorkspace
                anchors.fill: parent
                Behavior on opacity {
                    PropertyAnimation {
                        duration: 400
                    }
                }
                Component.onCompleted: {
                    workspaceManager.workspacesById.set(workspaceId, this)
                }

                Connections {
                    target: Helper
                    function onActivatedSurfaceChanged() {
                        // FIXME:When the layershell is closed, the focus cannot
                        //       be returned to the workspace. because the two are parallel layers.
                        if (!Helper.activatedSurface && container.visible) {
                            container.forceActiveFocus()
                        }
                    }
                }
            }
        }

        Loader {
            id: workspaceAnimation
            active: false
            anchors.fill: parent
            sourceComponent: WorkspaceAnimation {
                workspaceManager: root.workspaceManager
                onVisibleChanged: {
                    if (!visible) {
                        workspaceAnimation.active = false
                    }
                }
            }
        }

        DynamicCreatorComponent {
            id: inputPopupComponent
            creator: Helper.surfaceCreator
            chooserRole: "type"
            chooserRoleValue: "inputPopup"

            InputPopupSurface {
                required property WaylandInputPopupSurface popupSurface

                parent: getSurfaceItemFromWaylandSurface(popupSurface.parentSurface)?.surfaceItem
                id: inputPopupSurface
                shellSurface: popupSurface
            }
        }
    }

    DynamicCreatorComponent {
        id: layerComponent
        creator: Helper.surfaceCreator
        chooserRole: "type"
        chooserRoleValue: "layerShell"
        autoDestroy: false

        onObjectRemoved: function (obj) {
            obj.doDestroy()
        }

        LayerSurface {
            id: layerSurface
            creatorCompoment: layerComponent
            activeOutputItem: activeOutputDelegate
            focus: Helper.activatedSurface === wSurface
        }
    }

    Item {
        id: surfacesQObjParent
        visible: false

        DynamicCreatorComponent {
            id: toplevelComponent
            creator: Helper.surfaceCreator
            chooserRole: "type"
            chooserRoleValue: "toplevel"
            contextProperties: ({surfType: "xdg"}) // Object/QVariantMap type should use `({})`
            autoDestroy: false

            onObjectRemoved: function (obj) {
                obj.doDestroy()
            }

            SurfaceWrapper {
                id: wrapper
                creatorCompoment: toplevelComponent

                Repeater {
                    model: wrapper.outputs
                    delegate: Loader {
                        required property var modelData
                        active: wrapper.personalizationMapper.backgroundType === Personalization.Wallpaper
                        parent: wrapper.surfaceItem
                        z: wrapper.surfaceItem.contentItem.z - 2
                        anchors.fill: parent
                        sourceComponent: ShaderEffectSource {
                            WallpaperController {
                                id: wallpaperProxy
                                output: modelData
                            }
                            id: background
                            live: true
                            anchors.fill: parent
                            // TODO: multi screen coordinate
                            sourceRect: { Qt.rect(surfaceItem.x, surfaceItem.y, surfaceItem.width, surfaceItem.height) }
                            sourceItem: wallpaperProxy.proxy
                        }
                    }
                }

                Loader {
                    active: wrapper.personalizationMapper.backgroundType === Personalization.Blend
                    parent: wrapper.surfaceItem
                    anchors.fill: parent
                    z: wrapper.surfaceItem.contentItem.z - 2
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

                Connections {
                    target: Helper.xdgDecorationManager
                    function onSurfaceModeChanged(surface,mode) {
                        if (wrapper.wSurface.surface === surface) {
                            if (Helper.clientName(wSurface.surface) === "dde-desktop") {
                                wrapper.decoration.enable = false
                            } else {
                                wrapper.decoration.enable = mode === XdgDecorationManager.Server
                            }
                        }
                    }
                }

                Component.onCompleted: {
                    if (Helper.clientName(wSurface.surface) === "dde-desktop") {
                        wrapper.decoration.enable = false
                    } else {
                        wrapper.decoration.enable = Helper.xdgDecorationManager.modeBySurface(wrapper.wSurface.surface) === XdgDecorationManager.Server
                    }
                }
            }
        }

        DynamicCreatorComponent {
            id: popupComponent
            creator: Helper.surfaceCreator
            chooserRole: "type"
            chooserRoleValue: "popup"
            contextProperties: ({surfType: "xdg"})

            Item {
                id: popup

                required property WaylandXdgSurface wSurface
                property string type
                property int wid

                property var parentItem: root.getSurfaceItemFromWaylandSurface(wSurface.parentSurface)?.surfaceItem

                property alias surfaceItem: popupSurfaceItem

                parent: parentItem ?? root
                visible: parentItem?.effectiveVisible
                        && wSurface.surface.mapped && wSurface.WaylandSocket.rootSocket.enabled
                x: {
                    let retX = 0 // X coordinate relative to parent
                    let minX = 0
                    let maxX = root.width - popupSurfaceItem.width
                    if (!parentItem) {
                        retX = popupSurfaceItem.implicitPosition.x
                        if (retX > maxX)
                            retX = maxX
                        if (retX < minX)
                            retX = minX
                    } else {
                        retX = popupSurfaceItem.implicitPosition.x / parentItem.surfaceSizeRatio + parentItem.contentItem.x
                        let parentX = parent.mapToItem(root, 0, 0).x
                        if (retX + parentX > maxX) {
                            if (parentItem.type === "popup")
                                retX = retX - popupSurfaceItem.width - parent.width
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
                    let maxY = root.height - popupSurfaceItem.height
                    if (!parentItem) {
                        retY = popupSurfaceItem.implicitPosition.y
                        if (retY > maxY)
                            retY = maxY
                        if (retY < minY)
                            retY = minY
                    } else {
                        retY = popupSurfaceItem.implicitPosition.y / parentItem.surfaceSizeRatio + parentItem.contentItem.y
                        let parentY = parent.mapToItem(root, 0, 0).y
                        if (retY + parentY > maxY)
                            retY = maxY - parentY
                        if (retY + parentY < minY)
                            retY = minY - parentY
                    }
                    return retY
                }
                z: 201

                XdgSurfaceItem {
                    id: popupSurfaceItem
                    shellSurface: popup.wSurface

                    OutputLayoutItem {
                        anchors.fill: parent
                        layout: Helper.outputLayout

                        onEnterOutput: function(output) {
                            if (wSurface.surface) {
                                wSurface.surface.enterOutput(output)
                            }
                            Helper.onSurfaceEnterOutput(wSurface, popupSurfaceItem, output)
                        }
                        onLeaveOutput: function(output) {
                            wSurface.surface.leaveOutput(output)
                            Helper.onSurfaceLeaveOutput(wSurface, popupSurfaceItem, output)
                        }
                    }
                }
            }
        }

        DynamicCreatorComponent {
            id: xwaylandComponent
            creator: Helper.surfaceCreator
            chooserRole: "type"
            chooserRoleValue: "xwayland"
            contextProperties: ({surfType: "xwayland"})
            autoDestroy: false

            onObjectRemoved: function (obj) {
                obj.doDestroy()
            }
            SurfaceWrapper {
                creatorCompoment: xwaylandComponent
                property bool forcePositionToSurface: false
                readonly property XWaylandSurface asXSurface: {
                    wSurface as XWaylandSurface;
                }
                asXwayland.parentSurfaceItem: root.getSurfaceItemFromWaylandSurface(wSurface.parentXWaylandSurface)?.surfaceItem
                z: asXSurface.bypassManager ? 1 : 0 // TODO: make to enum type
                decoration.enable: !asXSurface.bypassManager
                                        && asXSurface.decorationsType !== XWaylandSurface.DecorationsNoBorder
                asXwayland.positionMode: {
                    if (!surfaceItem.effectiveVisible)
                        return XWaylandSurfaceItem.ManualPosition

                    if (surfaceItem.forcePositionToSurface)
                        return XWaylandSurfaceItem.PositionToSurface

                    return (Helper.movingItem === surfaceItem || surfaceItem.resizeMode === SurfaceItem.SizeToSurface)
                            ? XWaylandSurfaceItem.PositionToSurface
                            : XWaylandSurfaceItem.PositionFromSurface
                }
                function move(pos) {
                    forcePositionToSurface = true
                    manualMoveResizing = true
                    surfaceItem.x = pos.x
                    surfaceItem.y = pos.y
                    manualMoveResizing = false
                    forcePositionToSurface = false
                }

            }
        }
    }

    WorkspaceMask {
        id: mask

        anchors.fill: parent
        z: -96
    }

    WindowsSwitcher {
        id: switcher
        z: 100 + 1
        anchors {
            fill: parent
            topMargin: 30
            bottomMargin: 120
        }
        enabled: !multitaskView.active
        model: workspaceManager.workspacesById.get(workspaceManager.layoutOrder.get(currentWorkspaceId).wsid).surfaces
        visible: false // dbgswtchr.checked
        focus: false
        activeOutput: activeOutputDelegate
        onVisibleChanged: {
            if (switcher.visible) {
                mask.sourceComponent = mask.blackComponent
                if (switcher.enableAnimation) {
                    mask.itemOpacity = 0.0
                    mask.loaderStatus = 0
                    mask.loaderStatus = 1
                } else {
                    mask.itemOpacity = 0.5
                }
            }
        }

        onSurfaceActivated: (wrapper) => {
            wrapper.cancelMinimize()
            Helper.activatedSurface = wrapper.surfaceItem.shellSurface
        }

        onPreviewClicked: {
            unloadMask()
        }

        Binding {
            target: Helper
            property: "switcherEnabled"
            value: switcher.enabled
        }
        Binding {
            target: Helper
            property: "switcherOn"
            value: switcher.visible
        }
        Connections {
            target: Helper
            function onSwitcherOnChanged(on) {
                if (!on) {
                    switcher.handleExit()
                    unloadMask()
                }
            }
        }
    }

    Loader {
        id: multitaskView
        active: false
        focus: false
        anchors.fill: parent
        sourceComponent: Component {
            MultitaskView {
                anchors.fill: parent
                focus: false
                currentWorkspaceId: root.currentWorkspaceId
                setCurrentWorkspaceId: (id) => Helper.currentWorkspaceId = id
                onVisibleChanged: {
                    console.assert(!visible,'should be exit')
                    multitaskView.active = false
                }
            }
        }
    }

    Connections {
        target: ForeignToplevelV1
        function onRequestDockPreview(surfaces, target, abs, direction) {
            dockPreview.show(surfaces, getSurfaceItemFromWaylandSurface(target), abs, direction)
        }
        function onRequestDockClose() {
            dockPreview.close()
        }
    }

    DockPreview {
        id: dockPreview
        z: 100 + 1
        anchors.fill: parent
        visible: false
        onEntered: (relativeSurface) => {
            ForeignToplevelV1.enterDockPreview(relativeSurface)
        }
        onExited: (relativeSurface) => {
            ForeignToplevelV1.leaveDockPreview(relativeSurface)
        }
        onSurfaceActivated: (surfaceItem) => {
            surfaceItem.cancelMinimize()
            Helper.activatedSurface = surfaceItem.waylandSurface
        }
    }

    Connections {
        target: Helper
        function onSwitcherChanged(mode) {
            if (QmlHelper.workspaceManager.workspacesById.get(QmlHelper.workspaceManager.layoutOrder.get(Helper.currentWorkspaceId).wsid).surfaces.count > 0) {
                switcher.visible = true // ensure, won't emit event if already visible
                switch (mode) {
                case (Helper.Next):
                    switcher.next()
                    break
                case (Helper.Previous):
                    switcher.previous()
                    break
                }
            }
        }
    }

    Connections {
        target: Helper
        function onSwitcherActiveSwitch(mode) {
            if (!switcher.visible)
                return

            switch (mode) {
            case (Helper.Next):
                switcher.next()
                break
            case (Helper.Previous):
                switcher.previous()
                break
            }
        }
    }

    Connections {
        target: QmlHelper.shortcutManager
        function onMultitaskViewToggled() {
            if (!multitaskView.active) {
                multitaskView.active = true
                multitaskView.item.entry(MultitaskView.ActiveMethod.ShortcutKey)
            } else {
                multitaskView.item.exit()
            }
        }
        function onNextWorkspace() {
            const nWorkspaces = workspaceManager.layoutOrder.count
            const nextWorkspaceId = currentWorkspaceId + 1
            workspaceAnimation.active = true
            if (nextWorkspaceId >= nWorkspaces) {
                workspaceAnimation.item.addBounce(currentWorkspaceId, WorkspaceAnimation.Direction.Right)
            } else {
                workspaceAnimation.item.addAnimation(currentWorkspaceId, nextWorkspaceId)
                Helper.currentWorkspaceId = nextWorkspaceId
            }
        }
        function onPrevWorkspace() {
            const nWorkspaces = workspaceManager.layoutOrder.count
            const prevWorkspaceId = currentWorkspaceId - 1
            workspaceAnimation.active = true
            if (prevWorkspaceId < 0) {
                workspaceAnimation.item.addBounce(currentWorkspaceId, WorkspaceAnimation.Direction.Left)
            } else {
                workspaceAnimation.item.addAnimation(currentWorkspaceId, prevWorkspaceId)
                Helper.currentWorkspaceId = prevWorkspaceId
            }
        }
        function onJumpWorkspace(d) {
            const nWorkspaces = workspaceManager.layoutOrder.count
            if (d >= nWorkspaces) return
            workspaceAnimation.active = true
            workspaceAnimation.item.addAnimation(currentWorkspaceId, d)
            Helper.currentWorkspaceId = d
        }
        function onMoveToNeighborWorkspace(d, surface) {
            const nWorkspaces = workspaceManager.layoutOrder.count
            const surfaceWrapper = getSurfaceItemFromWaylandSurface(surface ?? Helper.activatedSurface)
            let relId = workspaceManager.workspacesById.get(surfaceWrapper.wid).workspaceRelativeId
            relId = (relId + d + nWorkspaces) % nWorkspaces
            surfaceWrapper.wid = workspaceManager.layoutOrder.get(relId).wsid
            // change workspace since no activatedSurface change
            Helper.currentWorkspaceId = relId
        }
    }
    Connections {
        target: Helper.multiTaskViewGesture

        onStatusChanged: (status) => {
            if (status === 1 || status === 2 || status ===3) {
                multitaskView.active = true
                multitaskView.item.entry(MultitaskView.ActiveMethod.Gesture)
            } else {
                multitaskView.active = false
            }
        }
    }
}

// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Waylib.Server
import TreeLand
import TreeLand.Protocols.ExtForeignToplevelList
import TreeLand.Protocols.ForeignToplevelManager
import TreeLand.Protocols.PersonalizationManager
import TreeLand.Protocols.OutputManagement
import TreeLand.Protocols.ShortcutManager
import TreeLand.Utils
import org.deepin.dtk 1.0 as D

FocusScope {
    id: root
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
                item: toplevel,
                type: "toplevel"
            }
        }

        let popup = QmlHelper.xdgSurfaceManager.getIf(popupComponent, finder)
        if (popup) {
            return {
                shell: popup,
                item: popup.xdgSurface,
                type: "popup"
            }
        }

        let layer = QmlHelper.layerSurfaceManager.getIf(layerComponent, finder)
        if (layer) {
            return {
                shell: layer,
                item: layer.surfaceItem,
                type: "layer"
            }
        }

        let xwayland = QmlHelper.xwaylandSurfaceManager.getIf(xwaylandComponent, finder)
        if (xwayland) {
            return {
                shell: xwayland,
                item: xwayland,
                type: "xwayland"
            }
        }

        return null
    }

    required property OutputDelegate activeOutputDelegate
    readonly property real switcherHideOpacity: 0.3

    property var workspaceManager: QmlHelper.workspaceManager
    property int currentWorkspaceId: 0

    Connections {
        target: workspaceManager.layoutOrder
        function onCountChanged() {
            if (currentWorkspaceId >= target.count) {
                currentWorkspaceId = target.count - 1
            }
        }
    }

    // activated workspace driven by surface activation
    property Item activatedSurfaceItem:
        getSurfaceItemFromWaylandSurface(Helper.activatedSurface)?.item || null // cannot assign [undefined] to QQuickItem*, need to assign null
    onActivatedSurfaceItemChanged: {
        if (activatedSurfaceItem?.parent?.workspaceRelativeId !== undefined)
            currentWorkspaceId = activatedSurfaceItem.parent.workspaceRelativeId
        QmlHelper.winposManager.updateSeq(Helper.activatedSurface.appId, activatedSurfaceItem.surfaceItem)
    }

    // activated surface driven by workspace change
    onCurrentWorkspaceIdChanged:
        if (activatedSurfaceItem?.parent?.workspaceRelativeId !== currentWorkspaceId)
            workspaceManager.workspacesById.get(workspaceManager.layoutOrder.get(currentWorkspaceId).wsid).selectSurfaceToActivate()

    FocusScope {
        anchors.fill: parent
        visible: !multitaskView.active
        enabled: !switcher.visible && !multitaskView.active
        focus: enabled
        opacity: if (switcher.visible || dockPreview.previewing) switcherHideOpacity
            else 1
        z: 0

        Behavior on opacity {
            NumberAnimation { duration: 300 }
        }

        Repeater {
            model: workspaceManager.layoutOrder
            anchors.fill: parent
            delegate: ToplevelContainer {
                objectName: `ToplevelContainer ${wsid}`
                required property int index
                required property int wsid
                property bool isCurrentWorkspace: workspaceRelativeId === currentWorkspaceId
                workspaceId: wsid
                workspaceRelativeId: index
                visible: isCurrentWorkspace
                focus: visible
                anchors.fill: parent
                Component.onCompleted: {
                    workspaceManager.workspacesById.set(workspaceId, this)
                }
            }
        }

        DynamicCreatorComponent {
            id: inputPopupComponent
            creator: QmlHelper.inputPopupSurfaceManager

            InputPopupSurface {
                required property InputMethodHelper inputMethodHelper
                required property WaylandInputPopupSurface popupSurface

                id: inputPopupSurface
                shellSurface: popupSurface
                helper: inputMethodHelper
            }
        }

    }

    Item {
        id: surfacesQObjParent
        visible: false

        DynamicCreatorComponent {
            id: toplevelComponent
            creator: QmlHelper.xdgSurfaceManager
            chooserRole: "type"
            chooserRoleValue: "toplevel"
            contextProperties: ({surfType: "xdg"}) // Object/QVariantMap type should use `({})`
            autoDestroy: false

            onObjectRemoved: function (obj) {
                obj.doDestroy()
            }

            SurfaceWrapper {
                id: wrapper
                ShaderEffectSource {
                    id: background
                    parent: wrapper.surfaceItem
                    z: wrapper.toplevelSurfaceItem.contentItem.z - 2
                    visible: false
                    anchors.fill: parent
                    sourceRect: { Qt.rect(toplevelSurfaceItem.x, toplevelSurfaceItem.y, toplevelSurfaceItem.width, toplevelSurfaceItem.height) }
                    sourceItem: personalizationMapper.backgroundImage
                }
                decoration.enable: surfaceDecorationMapper.serverDecorationEnabled
            }
        }

        DynamicCreatorComponent {
            id: popupComponent
            creator: QmlHelper.xdgSurfaceManager
            chooserRole: "type"
            chooserRoleValue: "popup"
            contextProperties: ({surfType: "xdg"})

            Item {
                id: popup

                required property WaylandXdgSurface waylandSurface
                property string type

                property alias xdgSurface: popupSurfaceItem
                property var parentItem: root.getSurfaceItemFromWaylandSurface(waylandSurface.parentSurface)?.item?.surfaceItem

                parent: parentItem ?? root
                visible: parentItem?.effectiveVisible
                        && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
                x: {
                    let retX = 0 // X coordinate relative to parent
                    let minX = 0
                    let maxX = root.width - xdgSurface.width
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
                                retX = retX - xdgSurface.width - parent.width
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
                    let maxY = root.height - xdgSurface.height
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
                    shellSurface: popup.waylandSurface

                    OutputLayoutItem {
                        anchors.fill: parent
                        layout: QmlHelper.layout

                        onEnterOutput: function(output) {
                            if (waylandSurface.surface) {
                                waylandSurface.surface.enterOutput(output)
                            }
                            Helper.onSurfaceEnterOutput(waylandSurface, popupSurfaceItem, output)
                        }
                        onLeaveOutput: function(output) {
                            waylandSurface.surface.leaveOutput(output)
                            Helper.onSurfaceLeaveOutput(waylandSurface, popupSurfaceItem, output)
                        }
                    }
                }
            }
            D.InWindowBlur {
                id: blur
                anchors.fill: parent
                z: toplevelSurfaceItem.contentItem.z - 2
                visible: personalizationMapper.backgroundType == 2
                radius: 16
            }
        }

        DynamicCreatorComponent {
            id: xwaylandComponent
            creator: QmlHelper.xwaylandSurfaceManager
            contextProperties: ({surfType: "xwayland"})
            autoDestroy: false

            onObjectRemoved: function (obj) {
                obj.doDestroy()
            }
            SurfaceWrapper {
                property bool forcePositionToSurface: false
                property var surfaceParent: root.getSurfaceItemFromWaylandSurface(waylandSurface.parentXWaylandSurface)
                asXwayland.parentSurfaceItem: surfaceParent ? surfaceParent.item : null
                z: waylandSurface.bypassManager ? 1 : 0 // TODO: make to enum type
                decoration.enable: !waylandSurface.bypassManager
                                        && waylandSurface.decorationsType !== XWaylandSurface.DecorationsNoBorder
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
                    isMoveResizing = true
                    surfaceItem.x = pos.x
                    surfaceItem.y = pos.y
                    isMoveResizing = false
                    forcePositionToSurface = false
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
            activeOutputItem: activeOutputDelegate
            focus: Helper.activatedSurface === this.waylandSurface
        }
    }

    WindowsSwitcher {
        id: switcher
        z: 100 + 1
        anchors.fill: parent
        enabled: !multitaskView.active
        model: workspaceManager.workspacesById.get(workspaceManager.layoutOrder.get(currentWorkspaceId).wsid).surfaces
        visible: false // dbgswtchr.checked
        focus: false
        activeOutput: activeOutputDelegate
        onSurfaceActivated: (wrapper) => {
            wrapper.cancelMinimize()
            Helper.activatedSurface = wrapper.surfaceItem.shellSurface
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
                if (!on) switcher.visible = false
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
                setCurrentWorkspaceId: (id) => root.currentWorkspaceId = id
                onVisibleChanged: {
                    console.assert(!visible,'should be exit')
                    multitaskView.active = false
                }
            }
        }
    }

    Connections {
        target: treelandForeignToplevelManager
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
            treelandForeignToplevelManager.enterDockPreview(relativeSurface)
        }
        onExited: (relativeSurface) => {
            treelandForeignToplevelManager.leaveDockPreview(relativeSurface)
        }
        onSurfaceActivated: (surfaceItem) => {
            surfaceItem.cancelMinimize()
            Helper.activatedSurface = surfaceItem.waylandSurface
        }
    }

    Connections {
        target: Helper
        function onSwitcherChanged(mode) {
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
    Connections {
        target: QmlHelper.shortcutManager
        function onMultitaskViewToggled() {
            multitaskView.active = !multitaskView.active
        }
        function onNextWorkspace() {
            const nWorkspaces = workspaceManager.layoutOrder.count
            currentWorkspaceId = (currentWorkspaceId + 1) % nWorkspaces
        }
        function onPrevWorkspace() {
            const nWorkspaces = workspaceManager.layoutOrder.count
            currentWorkspaceId = (currentWorkspaceId - 1 + nWorkspaces) % nWorkspaces
        }
        function onMoveToNeighborWorkspace(d) {
            const nWorkspaces = workspaceManager.layoutOrder.count
            const surfaceItem = getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
            let relId = workspaceManager.workspacesById.get(surfaceItem.workspaceId).workspaceRelativeId
            relId = (relId + d + nWorkspaces) % nWorkspaces
            surfaceItem.workspaceId = workspaceManager.layoutOrder.get(relId).wsid
            // change workspace since no activatedSurface change
            currentWorkspaceId = relId
        }
    }
}

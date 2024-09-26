// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import Waylib.Server
import TreeLand
import TreeLand.Utils
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

Item {
    id: root

    enum ActiveReason {
        ShortcutKey = 1,
        Gesture
    }

    enum ZOrder {
        Background = 0,
        Overlay = 1,
        FloatingItem = 2
    }

    readonly property int wsThumbMargin: 20
    readonly property int wsThumbHeight: 144
    readonly property int wsDelegateHeight: wsThumbHeight + wsThumbMargin * 2
    readonly property int wsThumbCornerRadius: 8
    readonly property int highlightBorderWidth: 4
    readonly property int maxWorkspace: 6
    readonly property int minWindowHeight: 232
    readonly property real titleBoxCornerRadius: 5

    required property int currentWorkspaceId
    required property var setCurrentWorkspaceId
    required property SlideAnimationController animationController
    property ListModel model: QmlHelper.workspaceManager.workspacesById.get(QmlHelper.workspaceManager.layoutOrder.get(currentWorkspaceId).wsid).surfaces
    property int animationDuration: 250
    property int animationEasing: Easing.OutExpo
    property int currentWsid // Used to store real current workspace id temporarily
    property bool exited: false
    property int activeReason: MultitaskView.ActiveReason.ShortcutKey
    property D.Palette outerShadowColor: DS.Style.highlightPanel.dropShadow

    property bool initialized: false
    readonly property QtObject taskViewGesture: Helper.multiTaskViewGesture
    property real taskviewVal: 0

    function enter(reason) {
        activeReason = reason
    }

    function exit(surfaceItem) {
        if (surfaceItem)
            Helper.activatedSurface = surfaceItem.shellSurface

        if (taskViewGesture.status === 3)
            taskViewGesture.toggle()

        exited = true
    }

    states: [
        State{
            name: "initial"
            PropertyChanges {
                target: root
                taskviewVal: 0
            }
        },
        State {
            name: "partial"
            PropertyChanges {
                target: root
                taskviewVal: taskViewGesture.partialGestureFactor
            }
        },
        State {
            name: "taskview"
            PropertyChanges {
                target: root
                taskviewVal: 1
            }
        }
    ]
    state: {
        if (!initialized) return "initial";

        if (exited) {
            if (taskviewVal === 0)
                root.visible = false;

            return "initial";
        }

        if (activeReason === MultitaskView.ActiveReason.ShortcutKey){
            return "taskview";
        } else {
            if (taskViewGesture.inProgress) return "partial";

            if (taskviewVal >=0.5) return "taskview";

            return "initial";
        }
    }
    transitions: Transition {
        to: "initial, taskview"
        NumberAnimation {
            duration: animationDuration
            property: "taskviewVal"
            easing.type: animationEasing
        }
    }

    QtObject {
        id: dragManager
        property Item item  // current dragged item
        property var accept // accept callback func
        property point destPoint
    }

    Item {
        id: outputsPlacementItem
        Repeater {
            model: Helper.outputLayout.outputs
            Item {
                id: outputPlacementItem
                required property OutputDelegate modelData
                function calcDisplayRect(item, output) {
                    // margins on this output
                    const margins = Helper.getOutputExclusiveMargins(output)
                    const coord = parent.mapFromItem(item, 0, 0)
                    // global pos before culling zones
                    const origin = Qt.rect(coord.x, coord.y, item.width, item.height)
                    return Qt.rect(origin.x + margins.left, origin.y + margins.top,
                        origin.width - margins.left - margins.right, origin.height - margins.top - margins.bottom)
                }
                property rect displayRect: root.visible ? calcDisplayRect(modelData, modelData.output) : Qt.rect(0, 0, 0, 0)
                x: displayRect.x
                y: displayRect.y
                width: displayRect.width
                height: displayRect.height
                readonly property real whRatio: width / height

                TapHandler {
                    gesturePolicy: TapHandler.WithinBounds
                    onTapped: root.exit()
                }

                DelegateModel {
                    id: visualModel
                    model: workspaceManager.layoutOrder
                    delegate: Item {
                        id: wsThumbItem
                        required property int wsid
                        required property int index
                        height: wsDelegateHeight
                        width: wsThumbHeight * outputPlacementItem.whRatio + 2 * wsThumbMargin
                        z: Drag.active ? 1 : 0
                        Drag.active: hdrg.active
                        Drag.onActiveChanged: {
                            if (Drag.active) {
                                dragManager.item = this
                                initialState = {x: x, y: y}
                                // Save current wsid here cause currentWorkspaceId should update after reordering
                                currentWsid = QmlHelper.workspaceManager.layoutOrder.get(currentWorkspaceId).wsid
                            } else {
                                if (dragManager.accept) {
                                    dragManager.accept()
                                } else {
                                    x = initialState.x
                                    y = initialState.y
                                    visualModel.items.move(DelegateModel.itemsIndex, index)
                                }
                                dragManager.item = null
                            }
                        }

                        DelegateModel.inPersistedItems: true
                        property var initialState
                        Rectangle {
                            anchors {
                                fill: parent
                                margins: wsThumbMargin - highlightBorderWidth
                            }
                            border.width: (!animationController.running && workspaceManager.workspacesById.get(wsid).isCurrentWorkspace) ? highlightBorderWidth : 0
                            border.color: "blue"
                            color: "transparent"
                            radius: wsThumbCornerRadius + highlightBorderWidth
                            Item {
                                id: content
                                anchors {
                                    fill: parent
                                    margins: highlightBorderWidth
                                }
                                clip: true

                                ShaderEffectSource {
                                    sourceItem: activeOutputDelegate
                                    anchors.fill: parent
                                    recursive: true
                                    hideSource: visible
                                }

                                ShaderEffectSource {
                                    sourceItem: workspaceManager.workspacesById.get(wsid)
                                    sourceRect: outputPlacementItem.displayRect
                                    anchors.fill: parent
                                    hideSource: visible
                                }

                                HoverHandler {
                                    id: hvrhdlr
                                    enabled: !hdrg.active
                                    onHoveredChanged: {
                                        if (hovered) {
                                            if (dragManager.item) {
                                                if (dragManager.item.source) {  // is dragging surface
                                                    if (dragManager.item.wrapper.wid !== wsid) {
                                                        dragManager.accept = () => {
                                                            dragManager.item.wrapper.wid = wsid
                                                        }
                                                    }
                                                } else {    // is dragging workspace
                                                    dragManager.destPoint = Qt.point(wsThumbItem.x, wsThumbItem.y)
                                                    dragManager.accept = () => {
                                                        const draggedItem = dragManager.item
                                                        const draggedWs = QmlHelper.workspaceManager.workspacesById.get(draggedItem.wsid)
                                                        const destIndex = draggedItem.DelegateModel.itemsIndex
                                                        QmlHelper.workspaceManager.layoutOrder.move(draggedWs.workspaceRelativeId, destIndex, 1)
                                                        const newCurrentWorkspaceIndex = QmlHelper.workspaceManager.workspacesById.get(currentWsid).workspaceRelativeId
                                                        root.setCurrentWorkspaceId(newCurrentWorkspaceIndex)
                                                        draggedItem.x = dragManager.destPoint.x
                                                        draggedItem.y = dragManager.destPoint.y
                                                    }
                                                    visualModel.items.move(dragManager.item.DelegateModel.itemsIndex, wsThumbItem.DelegateModel.itemsIndex)
                                                }
                                            }
                                        } else {
                                            if (dragManager.item?.source) // is dragging surface, workspace always lose hover
                                                dragManager.accept = null
                                        }
                                    }
                                }

                                TapHandler {
                                    id: taphdlr
                                    acceptedButtons: Qt.LeftButton
                                    enabled: !hdrg.active
                                    gesturePolicy: TapHandler.WithinBounds
                                    onTapped: {
                                        if (root.currentWorkspaceId === index)
                                            root.exit()
                                        else
                                            root.setCurrentWorkspaceId(index)
                                    }
                                }

                                TapHandler {
                                    id: quickHdlr
                                    acceptedButtons: Qt.RightButton
                                    enabled: !hdrg.active
                                    gesturePolicy: TapHandler.WithinBounds
                                    onTapped: {
                                        root.setCurrentWorkspaceId(index)
                                        root.exit()
                                    }
                                }

                                DragHandler {
                                    id: hdrg
                                    target: wsThumbItem
                                    yAxis.enabled: false
                                }
                            }

                            D.ItemViewport {
                                sourceItem: content
                                radius: wsThumbCornerRadius
                                anchors.fill: content
                                fixed: true
                                enabled: true
                                hideSource: true
                            }

                            D.RoundButton {
                                id: wsDestroyBtn
                                icon.name: "multitaskview_close"
                                icon.width: 26
                                icon.height: 26
                                height: 26
                                width: height
                                visible: (workspaceManager.layoutOrder.count > 1)
                                    && (hvrhdlr.hovered || hovered) && (dragManager.item === null) // FIXME: Fix destroy and add workspace logic
                                anchors {
                                    top: parent.top
                                    right: parent.right
                                    topMargin: -8
                                    rightMargin: -8
                                }
                                Item {
                                    id: control
                                    property D.Palette textColor: DS.Style.button.text
                                }
                                textColor: control.textColor
                                background: Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                }
                                onClicked: {
                                    workspaceManager.destroyWs(wsThumbItem.index)
                                }
                            }
                        }
                    }
                }

                Item {
                    id: workspacesListContainer
                    height: wsDelegateHeight
                    width: parent.width
                    z: MultitaskView.ZOrder.Overlay
                    transform: [
                        Translate {
                            y: height * (taskviewVal - 1.0)
                        }
                    ]

                    Item {
                        id: animationMask
                        property real localAnimationFactor: (wsThumbHeight * outputPlacementItem.whRatio+ 2 * wsThumbMargin) / animationController.refWrap
                        visible: animationController.running
                        anchors.fill: workspacesList
                        anchors.margins: wsThumbMargin - highlightBorderWidth
                        Rectangle {
                            width: wsThumbHeight * outputPlacementItem.whRatio + 2 * highlightBorderWidth
                            height: wsThumbHeight + 2 * highlightBorderWidth
                            border.width: highlightBorderWidth
                            border.color: "blue"
                            color: "transparent"
                            radius: wsThumbCornerRadius + highlightBorderWidth
                            x: animationController.viewportPos * animationMask.localAnimationFactor
                        }
                    }

                    ListView {
                        id: workspacesList
                        orientation: ListView.Horizontal
                        model: visualModel
                        height: parent.height
                        width: Math.min(parent.width,
                               model.count * (wsThumbHeight * outputPlacementItem.whRatio + 2 * wsThumbMargin))
                        anchors.horizontalCenter: parent.horizontalCenter
                        interactive: false
                        displaced: Transition {
                            NumberAnimation {
                                property: "x"
                                duration: animationDuration
                                easing.type: animationEasing
                            }
                        }
                    }

                    D.RoundButton {
                        id: wsCreateBtn
                        visible: workspaceManager.layoutOrder.count < maxWorkspace
                        anchors {
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            margins: 20
                        }
                        height: 80
                        width: 80
                        icon.name: "list_add"
                        icon.height: height
                        icon.width: width
                        background: Rectangle {
                            color: Qt.rgba(255, 255, 255, .4)
                            anchors.fill: parent
                            radius: 20
                        }
                        onClicked: {
                            workspaceManager.createWs()
                        }
                    }
                }

                WallpaperController {
                    id: wpCtrl
                    output: modelData.output
                }

                ShaderEffectSource {
                    z: MultitaskView.ZOrder.Background
                    id: wallpaper
                    live: true
                    smooth: true
                    sourceItem: wpCtrl.proxy
                    sourceRect: {
                        const margins = Helper.getOutputExclusiveMargins(modelData.output)
                        return Qt.rect(margins.left, margins.top, width, height)
                    }
                    anchors.fill: parent
                }

                RenderBufferBlitter {
                    z: MultitaskView.ZOrder.Background
                    id: blitter
                    anchors.fill: parent
                    opacity: taskviewVal
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

                Repeater {
                    id: wsDelegates
                    model: QmlHelper.workspaceManager.layoutOrder
                    Item {
                        id: wsDelegate
                        required property int index
                        required property int wsid
                        anchors.fill: parent
                        readonly property bool isCurrentWorkspace: index === currentWorkspaceId
                        visible: isCurrentWorkspace
                        z: MultitaskView.ZOrder.Background
                        Loader {
                            id: surfacesGridView
                            anchors {
                                fill: parent
                                topMargin: wsDelegateHeight
                            }
                            active: false
                            Component.onCompleted: {
                                // must after wslist's height stablized, so that surfaces animation is initialized correctly
                                active = true
                            }

                            sourceComponent: Item {
                                FilterProxyModel {
                                    id: outputProxy
                                    sourceModel: QmlHelper.workspaceManager.workspacesById.get(wsDelegate.wsid).surfaces
                                    property bool initialized: false
                                    filterAcceptsRow: (d) => {
                                                          const item = d.item
                                                          if (!(item instanceof SurfaceItem))
                                                          return false
                                                          return item.shellSurface.surface.primaryOutput === modelData.output
                                                      }
                                    onSourceModelChanged: {
                                        invalidate()
                                        if (initialized) grid.calcLayout()
                                    }
                                }

                                EQHGrid {
                                    id: grid
                                    width: parent.width
                                    height: parent.height
                                    model: outputProxy
                                    getRatio: (d) => d.item.width / d.item.height
                                    inProgress: !Number.isInteger(taskviewVal)
                                    partialGestureFactor: root.taskviewVal
                                    activeReason: root.activeReason
                                    animationDuration: root.animationDuration
                                    animationEasing: root.animationEasing
                                    exited: root.exited
                                    onRequestExit: {
                                        root.exit()
                                    }
                                    delegate: Item {
                                        id: surfaceItemDelegate
                                        property bool shouldPadding: needPadding
                                        D.BoxShadow {
                                            anchors.fill: paddingRect
                                            visible: paddingRect.visible
                                            shadowColor: root.D.ColorSelector.outerShadowColor
                                            shadowOffsetY: 2
                                            shadowBlur: 10
                                            cornerRadius: grid.delegateCornerRadius
                                            hollow: true
                                        }
                                        Rectangle {
                                            id: paddingRect
                                            visible: shouldPadding
                                            radius: grid.delegateCornerRadius
                                            anchors.fill: parent
                                            color: "white"
                                            opacity: paddingOpacity
                                        }
                                        property SurfaceItem source: modelData.item
                                        property SurfaceWrapper wrapper: modelData.wrapper
                                        property real ratio: source.width / source.height
                                        property real fullY
                                        clip: false
                                        onRatioChanged: if (grid.windowLoaded) {
                                            grid.calcLayout()
                                        }
                                        states: [
                                            State {
                                                name: "dragging"
                                                when: drg.active
                                                PropertyChanges {
                                                    restoreEntryValues: true // FIXME: does this restore propery binding?
                                                    surfaceItemDelegate {
                                                        parent: outputPlacementItem
                                                        x: mapToItem(outputPlacementItem, 0, 0).x
                                                        y: mapToItem(outputPlacementItem, 0, 0).y
                                                        z: MultitaskView.ZOrder.FloatingItem
                                                        scale: (Math.max(0, Math.min(drg.activeTranslation.y / fullY, 1)) * (100 - width) + width) / width
                                                        transformOrigin: Item.Center
                                                        shouldPadding: false
                                                    }
                                                }
                                            }
                                        ]

                                        function conv(y, item = parent) { // convert to outputPlacementItem's coord
                                            return mapToItem(outputPlacementItem, mapFromItem(item, 0, y)).y
                                        }

                                        property bool highlighted: dragManager.item === null && (hvhdlr.hovered || surfaceCloseBtn.hovered) && !root.exited
                                        HoverHandler {
                                            id: hvhdlr
                                            enabled: !drg.active
                                        }
                                        TapHandler {
                                            gesturePolicy: TapHandler.WithinBounds
                                            onTapped: root.exit(source)
                                        }
                                        DragHandler {
                                            id: drg
                                            onActiveChanged: {
                                                if (active) {
                                                    dragManager.item = parent
                                                } else {
                                                    if (dragManager.accept) {
                                                        dragManager.accept()
                                                    }
                                                    dragManager.item = null
                                                }
                                            }
                                            onGrabChanged: (transition, eventPoint) => {
                                                               switch (transition) {
                                                                   case PointerDevice.GrabExclusive:
                                                                   fullY = conv(workspacesList.height, workspacesList) - conv(mapToItem(surfaceItemDelegate, eventPoint.position).y, surfaceItemDelegate)
                                                                   break
                                                               }
                                                           }
                                        }

                                        Item {
                                            anchors.fill: parent
                                            clip: shouldPadding
                                            Item {
                                                id: surfaceShot
                                                width: parent.width
                                                height: width / ratio
                                                anchors.centerIn: parent

                                                D.BoxShadow {
                                                    anchors.fill: parent
                                                    shadowColor: root.D.ColorSelector.outerShadowColor
                                                    shadowOffsetY: 2
                                                    shadowBlur: 10
                                                    cornerRadius: grid.delegateCornerRadius
                                                    hollow: true
                                                }

                                                ShaderEffectSource {
                                                    id: preview
                                                    anchors.fill: parent
                                                    live: true
                                                    // no hidesource, may conflict with workspace thumb
                                                    smooth: true
                                                    sourceItem: source
                                                    visible: false
                                                }

                                                MultiEffect {
                                                    enabled: grid.delegateCornerRadius > 0
                                                    anchors.fill: preview
                                                    source: preview
                                                    maskEnabled: true
                                                    maskSource: mask
                                                }

                                                Item {
                                                    id: mask
                                                    anchors.fill: preview
                                                    layer.enabled: true
                                                    visible: false
                                                    Rectangle {
                                                        anchors.fill: parent
                                                        radius: grid.delegateCornerRadius
                                                    }
                                                }
                                            }
                                        }

                                        Rectangle {
                                            anchors {
                                                fill: parent
                                                topMargin: -highlightBorderWidth
                                                bottomMargin: -highlightBorderWidth
                                                leftMargin: -highlightBorderWidth
                                                rightMargin: -highlightBorderWidth
                                            }

                                            color: "transparent"
                                            border.width: highlighted ? highlightBorderWidth : 0
                                            border.color: "blue"
                                            radius: grid.delegateCornerRadius + highlightBorderWidth
                                        }

                                        D.RoundButton {
                                            id: surfaceCloseBtn
                                            icon.name: "multitaskview_close"
                                            icon.width: 26
                                            icon.height: 26
                                            height: 26
                                            width: height
                                            visible: surfaceItemDelegate.highlighted
                                            anchors {
                                                top: parent.top
                                                right: parent.right
                                                topMargin: -8
                                                rightMargin: -8
                                            }
                                            Item {
                                                id: surfaceCloseBtnControl
                                                property D.Palette textColor: DS.Style.button.text
                                            }
                                            textColor: surfaceCloseBtnControl.textColor
                                            background: Rectangle {
                                                anchors.fill: parent
                                                color: "transparent"
                                            }
                                            onClicked: {
                                                surfaceItemDelegate.visible = false
                                                wrapper.decoration.requestClose()
                                            }
                                        }

                                        Control {
                                            id: titleBox
                                            anchors {
                                                bottom: parent.bottom
                                                horizontalCenter: parent.horizontalCenter
                                                margins: 10
                                            }
                                            width: Math.min(implicitContentWidth + 2 * padding, parent.width * .7)
                                            padding: 10
                                            // Should be invisible or it sometimes steals event point from mouse
                                            visible: highlighted && source.shellSurface.title !== ""

                                            contentItem: Text {
                                                text: source.shellSurface.title
                                                elide: Qt.ElideRight
                                            }
                                            background: Rectangle {
                                                color: Qt.rgba(255, 255, 255, .2)
                                                radius: titleBoxCornerRadius
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Loader {
                    id: workspaceAnimationLoader
                    active: false
                    Connections {
                        target: animationController
                        function onRunningChanged() {
                            if (animationController.running) workspaceAnimationLoader.active = true
                        }
                    }
                    z: MultitaskView.ZOrder.Background
                    anchors.fill: parent
                    sourceComponent: Item {
                        id: animationDelegate
                        visible: animationController.running
                        onVisibleChanged: {
                            if (!visible) {
                                workspaceAnimationLoader.active = false
                            }
                        }
                        property real localFactor: width / animationController.refWidth

                        Rectangle {
                            anchors.fill: parent
                            color: "black"
                        }
                        Row {
                            visible: true
                            spacing: animationController.refGap * animationDelegate.localFactor
                            x: -animationController.viewportPos * animationDelegate.localFactor
                            Repeater {
                                model: QmlHelper.workspaceManager.layoutOrder
                                Item {
                                    id: wsShot
                                    required property int index
                                    width: animationDelegate.width
                                    height: animationDelegate.height
                                    ShaderEffectSource {
                                        z: MultitaskView.ZOrder.Background
                                        id: wallpaperShot
                                        live: true
                                        smooth: true
                                        sourceItem: wpCtrl.proxy
                                        sourceRect: {
                                            const margins = Helper.getOutputExclusiveMargins(modelData.output)
                                            return Qt.rect(margins.left, margins.top, width, height)
                                        }
                                        anchors.fill: parent
                                    }

                                    RenderBufferBlitter {
                                        z: MultitaskView.ZOrder.Background
                                        id: shotBlitter
                                        anchors.fill: parent
                                        MultiEffect {
                                            id: shotBlur
                                            anchors.fill: parent
                                            source: shotBlitter.content
                                            autoPaddingEnabled: false
                                            blurEnabled: true
                                            blur: 1.0
                                            blurMax: 64
                                            saturation: 0.2
                                        }
                                    }

                                    ShaderEffectSource {
                                        id: ws
                                        visible: true
                                        anchors.fill: parent
                                        hideSource: visible
                                        sourceItem: wsDelegates.itemAt(index) ?? null
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    Component.onCompleted: {
        initialized = true
    }
}

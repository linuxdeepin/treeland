// Copyright (C) 2024 Yicheng Zhong <zhongyicheng@uniontech.com>.
// SPDX-License-Identif ier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server
import TreeLand
import TreeLand.Utils
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

Item {
    id: root
    required property int currentWorkspaceId
    required property var setCurrentWorkspaceId
    property ListModel model: QmlHelper.workspaceManager.workspacesById.get(QmlHelper.workspaceManager.layoutOrder.get(currentWorkspaceId).wsid).surfaces
    property int current: -1
    property bool exited: false // flag controls exit animation
    property int fadeDuration: 250
    function exit(surfaceItem) {
        if (surfaceItem)
            Helper.activatedSurface = surfaceItem.shellSurface
        else if (current >= 0 && current < model.count)
            Helper.activatedSurface = model.get(current).item.shellSurface
        exited = true
    }

    property int currentWsid // Used to store real current workspace id temporarily
    states: [
        State {
            name: "exited"
            when: exited
            PropertyChanges {
                root {
                    visible: false
                }
            }
        }
    ]
    transitions: [
        Transition {
            from: ""
            to: "exited"
            PropertyAnimation {
                property: "visible"
                duration: fadeDuration
            }
        }
    ]

    QtObject {
        id: dragManager
        property Item item  // current dragged item
        property var accept // accept callback func
    }

    Item {
        id: outputsPlacementItem
        Repeater {
            model: Helper.outputLayout.outputs
            Item {
                id: outputPlacementItem
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
                HoverHandler {
                    id: rootHvrHdlr
                }

                TapHandler {
                    gesturePolicy: TapHandler.WithinBounds
                    onTapped: root.exit()
                }

                Item {
                    anchors.fill: parent
                    DelegateModel {
                        id: visualModel
                        model: workspaceManager.layoutOrder
                        delegate: Item {
                            id: wsThumbItem
                            required property int wsid
                            required property int index
                            height: workspacesList.height
                            width: height * outputPlacementItem.width / outputPlacementItem.height
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
                                    margins: 16
                                }
                                border.width: workspaceManager.workspacesById.get(wsid).isCurrentWorkspace ? 4 : 0
                                border.color: "blue"
                                color: "transparent"
                                radius: 12
                                Item {
                                    id: content
                                    anchors {
                                        fill: parent
                                        margins: 4
                                    }
                                    clip: true
                                    ShaderEffectSource {
                                        sourceItem: activeOutputDelegate
                                        anchors.fill: parent
                                        recursive: true
                                    }
                                    ShaderEffectSource {
                                        sourceItem: workspaceManager.workspacesById.get(wsid)
                                        sourceRect: outputPlacementItem.displayRect
                                        anchors.fill: parent
                                        recursive: true
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        color: hvrhdlr.hovered ? Qt.rgba(0, 0, 0, .2) : Qt.rgba(0, 0, 0, 0)
                                        Text {
                                            anchors.centerIn: parent
                                            color: "white"
                                            text: `No.${index}`
                                        }
                                    }
                                    HoverHandler {
                                        id: hvrhdlr
                                        enabled: !hdrg.active
                                        onHoveredChanged: {
                                            if (hovered) {
                                                if (dragManager.item) {
                                                    if (dragManager.item.source) {  // is dragging surface
                                                        dragManager.accept = () => {
                                                            dragManager.item.source.workspaceId = wsid
                                                            dragManager.item.source.parent = QmlHelper.workspaceManager.workspacesById.get(wsid)
                                                        }
                                                    } else {    // is dragging workspace
                                                        dragManager.accept = () => {
                                                            const draggedItem = dragManager.item
                                                            const draggedWs = QmlHelper.workspaceManager.workspacesById.get(draggedItem.wsid)
                                                            const destIndex = draggedItem.DelegateModel.itemsIndex
                                                            QmlHelper.workspaceManager.layoutOrder.move(draggedWs.workspaceRelativeId, destIndex, 1)
                                                            // make curWorkspaceId follow the workspace
                                                            const newCurrentWorkspaceIndex = QmlHelper.workspaceManager.workspacesById.get(currentWsid).workspaceRelativeId
                                                            root.setCurrentWorkspaceId(newCurrentWorkspaceIndex)
                                                            // reset dragged item's align
                                                            visualModel.items.move(destIndex, destIndex)
                                                            draggedItem.y = draggedItem.initialState.y  // y is not set by layout?
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
                                        enabled: !hdrg.active
                                        gesturePolicy: TapHandler.WithinBounds
                                        onTapped: {
                                            if (root.currentWorkspaceId === index)
                                                root.exit()
                                            else
                                                root.setCurrentWorkspaceId(index)
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
                                    radius: 8
                                    anchors.fill: content
                                    fixed: true
                                    enabled: true
                                    hideSource: true
                                }

                                // D.RoundButton {
                                //     id: wsDestroyBtn
                                //     icon.name: "window_close"
                                //     icon.width: 26
                                //     icon.height: 26
                                //     height: 26
                                //     width: height
                                //     visible: (workspaceManager.layoutOrder.count > 1)
                                //         && (hvrhdlr.hovered || hovered)
                                //     anchors {
                                //         top: parent.top
                                //         right: parent.right
                                //         topMargin: -8
                                //         rightMargin: -8
                                //     }
                                //     Item {
                                //         id: control
                                //         property D.Palette textColor: DS.Style.button.text
                                //     }
                                //     textColor: control.textColor
                                //     background: Rectangle {
                                //         anchors.fill: parent
                                //         color: "white"
                                //         radius: parent.height / 2
                                //     }
                                //     onClicked: {
                                //         workspaceManager.destroyWs(parent.index)
                                //         root.model = QmlHelper.workspaceManager.workspacesById.get(QmlHelper.workspaceManager.layoutOrder.get(currentWorkspaceId).wsid).surfaces
                                //     }
                                // }
                            }
                        }
                    }
                    Item {
                        id: workspacesListContainer
                        height: outputPlacementItem.height * .2
                        width: parent.width

                        NumberAnimation on y {
                            from: -height
                            to: 0
                            duration: fadeDuration
                            easing.type: Easing.OutCubic
                        }

                        states: [
                            State {
                                name: "exited"
                                when: root.exited
                                PropertyChanges {
                                    workspacesListContainer {
                                        y: -height
                                    }
                                }
                            }
                        ]

                        transitions: [
                            Transition {
                                from: ""
                                to: "exited"
                                NumberAnimation {
                                    property: "y"
                                    duration: fadeDuration
                                    easing.type: Easing.InCubic
                                }
                            }
                        ]

                        ListView {
                            id: workspacesList
                            orientation: ListView.Horizontal
                            model: visualModel
                            height: parent.height
                            width: Math.min(parent.width,
                                model.count * height * outputPlacementItem.width / outputPlacementItem.height)
                            anchors.horizontalCenter: parent.horizontalCenter
                            interactive: false
                        }
                        // D.RoundButton {
                        //     id: wsCreateBtn
                        //     anchors {
                        //         right: parent.right
                        //         verticalCenter: parent.verticalCenter
                        //         margins: 20
                        //     }
                        //     height: 80
                        //     width: 80
                        //     icon.name: "list_add"
                        //     icon.height: height
                        //     icon.width: width
                        //     background: Rectangle {
                        //         color: Qt.rgba(255, 255, 255, .4)
                        //         anchors.fill: parent
                        //         radius: 20
                        //     }
                        //     onClicked: {
                        //         workspaceManager.createWs()
                        //     }
                        // }
                    }
                    Item {
                        y: outputPlacementItem.height * .2
                        width: parent.width
                        height: outputPlacementItem.height * .8
                        Loader {
                            id: surfacesGridView
                            anchors {
                                fill: parent
                                margins: 30
                            }
                            active: false
                            Component.onCompleted: {
                                // must after wslist's height stablized, so that surfaces animation is initialized correctly
                                active = true
                            }

                            sourceComponent: Item {
                                FilterProxyModel {
                                    id: outputProxy
                                    sourceModel: root.model
                                    property bool initialized: false
                                    filterAcceptsRow: (d) => {
                                                          const item = d.item
                                                          if (!(item instanceof SurfaceItem))
                                                          return false
                                                          return item.shellSurface.surface.primaryOutput === modelData.output
                                                      }
                                    Component.onCompleted: {
                                        initialized = true  // TODO better initialize timing
                                        invalidate()
                                        grid.calcLayout()
                                    }
                                    onSourceModelChanged: {
                                        invalidate()
                                        if (initialized) grid.calcLayout()
                                    }
                                }

                                EQHGrid {
                                    id: grid
                                    anchors.fill: parent
                                    model: outputProxy
                                    minH: 100
                                    maxH: parent.height
                                    maxW: parent.width
                                    availH: parent.height
                                    availW: parent.width
                                    getRatio: (d) => d.item.width / d.item.height
                                    enterAnimationEnabled: true
                                    exited: root.exited
                                    delegate: Item {
                                        id: surfaceItemDelegate
                                        property SurfaceItem source: modelData.item
                                        property SurfaceWrapper wrapper: modelData.wrapper
                                        property real ratio: source.width / source.height
                                        onRatioChanged: {
                                            grid.calcLayout()
                                        }

                                        property var initialState
                                        property real animRatio: 1
                                        property point animationAnchor
                                        property real invariantXRatio
                                        property real invariantYRatio
                                        property real destY
                                        property real fullY
                                        function conv(y, item = parent) { // convert to outputPlacementItem's coord
                                            return mapToItem(outputPlacementItem, mapFromItem(item, 0, y)).y
                                        }

                                        Connections {
                                            target: rootHvrHdlr
                                            function onPointChanged() {
                                                if (drg.active) {
                                                    const mX = rootHvrHdlr.point.position.x
                                                    const mY = rootHvrHdlr.point.position.y
                                                    const destW = 100
                                                    const cursor = mapToItem(surfaceItemDelegate.parent, mapFromItem(outputPlacementItem, mX, mY))
                                                    const deltY = Math.max(Math.min(mY - destY, fullY), 0)
                                                    animRatio = (((fullY - deltY) / fullY) * (destW - initialState.width) + initialState.width) / initialState.width
                                                    surfaceItemDelegate.x = cursor.x - width * invariantXRatio
                                                    surfaceItemDelegate.y = cursor.y - height * invariantYRatio
                                                }
                                            }
                                        }

                                        width: displayWidth * animRatio
                                        height: width * source.height / source.width
                                        // clip: true
                                        z: drg.active ? 1 : 0   // dragged item should float
                                        property bool highlighted: dragManager.item !== this && (hvhdlr.hovered || surfaceCloseBtn.hovered) && !root.exited
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
                                            onActiveChanged: if (active) {
                                                                 dragManager.item = parent
                                                                 initialState = {x: parent.x, y: parent.y, width: parent.width}
                                                             } else {
                                                                 if (dragManager.accept) {
                                                                     dragManager.accept()
                                                                 } else {
                                                                     parent.x = initialState.x
                                                                     parent.y = initialState.y
                                                                     parent.animRatio = 1
                                                                 }
                                                                 dragManager.item = null
                                                             }
                                            onGrabChanged: (transition, eventPoint) => {
                                                               switch (transition) {
                                                                   case PointerDevice.GrabExclusive:
                                                                   animationAnchor = mapToItem(surfaceItemDelegate, eventPoint.position)
                                                                   invariantXRatio = animationAnchor.x / surfaceItemDelegate.width
                                                                   invariantYRatio = animationAnchor.y / surfaceItemDelegate.height
                                                                   destY = conv(workspacesList.height, workspacesList)
                                                                   fullY = conv(animationAnchor.y, surfaceItemDelegate) - destY
                                                                   break
                                                               }
                                                           }
                                        }
                                        Rectangle {
                                            anchors.fill: parent
                                            color: "transparent"
                                            border.width: highlighted ? 4 : 0
                                            border.color: "blue"
                                            radius: wrapper.decoration.radius + border.width
                                        }

                                        ShaderEffectSource {
                                            anchors {
                                                fill: parent
                                                margins: 4
                                            }
                                            live: true
                                            // no hidesource, may conflict with workspace thumb
                                            smooth: true
                                            sourceItem: source
                                        }

                                        D.RoundButton {
                                            id: surfaceCloseBtn
                                            icon.name: "window_close"
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
                                                color: "white"
                                                radius: parent.height / 2
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
                                                radius: 5
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

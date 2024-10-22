// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import org.deepin.dtk as D
import org.deepin.dtk.style as DS
import Treeland

Item {
    id: root

    required property QtObject output
    required property QtObject dragManager
    required property Multitaskview multitaskview
    readonly property real whRatio: output.outputItem.width / output.outputItem.height

    height: TreelandConfig.workspaceDelegateHeight
    width: parent.width
    z: Multitaskview.Overlay
    transform: [
        Translate {
            y: height * (multitaskview.taskviewVal - 1.0)
        }
    ]

    Item {
        id: animationMask
        property real localAnimationFactor: (TreelandConfig.workspaceThumbHeight * root.whRatio+ 2 * TreelandConfig.workspaceThumbMargin)
                                            / Helper.workspace.animationController.refWrap
        visible: Helper.workspace.animationController.running
        anchors.fill: workspaceList
        anchors.margins: TreelandConfig.workspaceThumbMargin - TreelandConfig.highlightBorderWidth
        Rectangle {
            width: TreelandConfig.workspaceThumbHeight * root.whRatio + 2 * TreelandConfig.highlightBorderWidth
            height: TreelandConfig.workspaceThumbHeight + 2 * TreelandConfig.highlightBorderWidth
            border.width: TreelandConfig.highlightBorderWidth
            border.color: "blue"
            color: "transparent"
            radius: TreelandConfig.workspaceThumbCornerRadius + TreelandConfig.highlightBorderWidth
            x: Helper.workspace.animationController.viewportPos * animationMask.localAnimationFactor
        }
    }

    DelegateModel {
        id: visualModel
        model: Helper.workspace.models
        delegate: Item {
            required property WorkspaceModel workspace
            required property int index
            id: workspaceThumbDelegate
            height: TreelandConfig.workspaceDelegateHeight
            width: TreelandConfig.workspaceThumbHeight * root.whRatio + 2 * TreelandConfig.workspaceThumbMargin
            Drag.active: hdrg.active
            z: Drag.active ? 1 : 0
            property point restorePos
            Drag.onActiveChanged: {
                if (Drag.active) {
                    dragManager.item = this
                    dragManager.destPoint = Qt.point(x, y)
                } else {
                    if (dragManager.accept) {
                        dragManager.accept()
                    }
                    x = dragManager.destPoint.x
                    y = dragManager.destPoint.y
                    dragManager.item = null
                }
            }

            DelegateModel.inPersistedItems: true
            Rectangle {
                anchors {
                    fill: parent
                    margins: TreelandConfig.workspaceThumbMargin - TreelandConfig.highlightBorderWidth
                }
                border.width: (!Helper.workspace.animationController.running && workspace.visible) ? TreelandConfig.highlightBorderWidth : 0
                border.color: "blue"
                color: "transparent"
                radius: TreelandConfig.workspaceThumbCornerRadius + TreelandConfig.highlightBorderWidth
                Item {
                    id: content
                    anchors {
                        fill: parent
                        margins: TreelandConfig.highlightBorderWidth
                    }
                    clip: true
                    // For TRadiusEffect
                    layer.enabled: true
                    opacity: 0

                    WallpaperController {
                        id: wpCtrl
                        output: root.output.outputItem.output
                        lock: true
                        type: WallpaperController.Normal
                    }

                    ShaderEffectSource {
                        sourceItem: wpCtrl.proxy
                        anchors.fill: parent
                        recursive: true
                        hideSource: false
                    }

                    WorkspaceProxy {
                        id: wp
                        workspace: workspaceThumbDelegate.workspace
                        output: root.output
                    }

                    ShaderEffectSource {
                        sourceItem: wp
                        anchors.fill: parent
                        recursive: true
                        hideSource: true
                    }

                    HoverHandler {
                        id: hvrhdlr
                        enabled: !hdrg.active
                        onHoveredChanged: {
                            if (hovered) {
                                if (dragManager.item) {
                                    if (dragManager.item.workspace) {  // is dragging workspace
                                        dragManager.destPoint = Qt.point(workspaceThumbDelegate.x, workspaceThumbDelegate.y)
                                        visualModel.items.move(dragManager.item.DelegateModel.itemsIndex, workspaceThumbDelegate.DelegateModel.itemsIndex)
                                        dragManager.accept = () => {
                                            const fromIndex = dragManager.item.index
                                            const destIndex = workspaceThumbDelegate.index
                                            Helper.workspace.moveModelTo(dragManager.item.workspace.id, destIndex)
                                        }
                                    } else {    // is dragging surface
                                        if (workspace.id !== dragManager.item.wrapper.workspaceId) {
                                            dragManager.accept = () => {
                                                Helper.workspace.moveSurfaceTo(dragManager.item.wrapper, workspace.id)
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (dragManager.item?.wrapper) // is dragging surface, workspace always lose hover
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
                            if (workspace === Helper.workspace.current) {
                                multitaskview.exit()
                            } else {
                                Helper.workspace.switchTo(index)
                            }
                        }
                    }

                    TapHandler {
                        id: quickHdlr
                        acceptedButtons: Qt.RightButton
                        enabled: !hdrg.active
                        gesturePolicy: TapHandler.WithinBounds
                        onTapped: {
                            Helper.workspace.switchTo(index)
                            multitaskview.exit()
                        }
                    }

                    DragHandler {
                        id: hdrg
                        target: workspaceThumbDelegate
                        yAxis.enabled: false
                    }
                }

                TRadiusEffect {
                    sourceItem: content
                    radius: TreelandConfig.workspaceThumbCornerRadius
                    anchors.fill: content
                    hideSource: true
                }

                D.RoundButton {
                    id: wsDestroyBtn
                    icon.name: "multitaskview_close"
                    icon.width: 26
                    icon.height: 26
                    height: 26
                    width: height
                    visible: (Helper.workspace.count > 1)
                             && (hvrhdlr.hovered || hovered) && dragManager.item === null
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
                        Helper.workspace.removeModel(workspace.index)
                    }
                }
            }
        }
    }


    ListView {
        id: workspaceList
        anchors {
            top: parent.top
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
        }

        interactive: false
        currentIndex: Helper.workspace.currentIndex
        highlightFollowsCurrentItem: true
        displaced: Transition {
            NumberAnimation {
                property: "x"
                duration: TreelandConfig.multitaskviewAnimationDuration
                easing.type: TreelandConfig.multitaskviewEasingCurveType
            }
        }
        width: Math.min(parent.width,
                        Helper.workspace.count * (TreelandConfig.workspaceThumbHeight * root.whRatio + 2 * TreelandConfig.workspaceThumbMargin))
        orientation: ListView.Horizontal
        height: TreelandConfig.workspaceDelegateHeight
        model: visualModel
    }
    Item {
        id: newWorkspaceDropArea
        anchors {
            left: workspaceList.right
            right: root.right
            top: root.top
            bottom: root.bottom
        }
        HoverHandler {
            onHoveredChanged: {
                // TODO: add workspace if needed
            }
        }
    }

    D.RoundButton {
        id: wsCreateBtn
        visible: Helper.workspace.count < TreelandConfig.maxWorkspace
        anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter
            margins: 20
        }
        height: 80
        width: 80
        icon.name: "button_add"
        icon.height: 26
        icon.width: 26
        background: Rectangle {
            color: Qt.rgba(255, 255, 255, .4)
            anchors.fill: parent
            radius: 20
        }
        onClicked: {
            Helper.workspace.createModel(`workspace-${Helper.workspace.count}`, false)
        }
    }
}

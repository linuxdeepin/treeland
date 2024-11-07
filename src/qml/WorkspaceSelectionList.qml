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

    Timer {
        id: switchTimer
        interval: 500
        property int switchToIndex: -1
        property WorkspaceModel switchToModel: null
        onTriggered: {
            if (dragManager.doNotRestoreAccept) {
                // Substitute switch workspace
                dragManager.accept = () => {
                    switchTimer.stop()
                    Helper.workspace.moveSurfaceTo(dragManager.item.wrapper, switchTimer.switchToModel.id)
                }
            } else {
                dragManager.doNotRestoreAccept = true
            }
            Helper.workspace.switchTo(switchTimer.switchToIndex)
        }
    }

    function restartSwitchTimer(index: int, workspace: WorkspaceModel) {
        switchTimer.switchToIndex = index
        switchTimer.switchToModel = workspace
        switchTimer.restart()
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
                    dragManager.doNotRestoreAccept = false
                }
            }

            Text {
                id: deleteHint
                text: qsTr("Release to delete")
                anchors.fill: parent
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                font.pixelSize: 30
                visible: -vdrg.activeTranslation.y > (height / 2)
            }

            DelegateModel.inPersistedItems: true
            Rectangle {
                id: container
                anchors {
                    fill: parent
                    margins: TreelandConfig.workspaceThumbMargin - TreelandConfig.highlightBorderWidth
                }
                border.width: (!Helper.workspace.animationController.running && workspace.visible) ? TreelandConfig.highlightBorderWidth : 0
                border.color: "blue"
                color: "transparent"
                radius: TreelandConfig.workspaceThumbCornerRadius + TreelandConfig.highlightBorderWidth
                states: [
                    State {
                        name: "dragging"
                        when: vdrg.active || hdrg.active
                        PropertyChanges {
                            container {
                                anchors.fill: undefined
                            }
                        }
                    }
                ]

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
                                        restartSwitchTimer(workspaceThumbDelegate.index, workspaceThumbDelegate.workspace)
                                        if (workspace.id !== dragManager.item.wrapper.workspaceId && !dragManager.doNotRestoreAccept) {
                                            dragManager.accept = () => {
                                                switchTimer.stop()
                                                Helper.workspace.moveSurfaceTo(dragManager.item.wrapper, workspace.id)
                                            }
                                        }
                                    }
                                }
                            } else {
                                switchTimer.stop()
                                if (dragManager.item?.wrapper && !dragManager.doNotRestoreAccept) // is dragging surface, workspace always lose hover
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
                        enabled: !vdrg.active
                        target: workspaceThumbDelegate
                        yAxis.enabled: false
                    }

                    DragHandler {
                        id: vdrg
                        enabled: !hdrg.active && Helper.workspace.count > 1
                        property bool commitDeletion: deleteHint.visible
                        target: container
                        xAxis.enabled: false
                        yAxis {
                            maximum: TreelandConfig.workspaceThumbMargin - TreelandConfig.highlightBorderWidth
                            minimum: -container.height
                        }
                        onActiveChanged: {
                            if (!active && commitDeletion) {
                                Helper.workspace.removeModel(workspaceThumbDelegate.index)
                            }
                        }
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
                             && (hvrhdlr.hovered || hovered) && dragManager.item === null && !vdrg.active
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
                        Helper.workspace.removeModel(workspaceThumbDelegate.index)
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
        highlightFollowsCurrentItem: false
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
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function (event) {
                const directionDelta = event.inverted ? -event.angleDelta.y: event.angleDelta.y
                if (directionDelta < 0) {
                    Helper.workspace.switchToNext()
                } else {
                    Helper.workspace.switchToPrev()
                }
            }
        }
    }

    D.RoundButton {
        id: wsCreateBtn
        visible: Helper.workspace.count < TreelandConfig.maxWorkspace
        anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter
            margins: 40
        }
        height: 64
        width: 64
        icon.name: "add"
        icon.height: 22
        icon.width: 22
        background: Item {
            Rectangle {
                id: bgRect
                color: Qt.rgba(16, 16, 16, .1)
                anchors.fill: parent
                radius: 10
            }
            Blur {
                anchors.fill: bgRect
                radius: 10
            }
            Border {
                anchors.fill: parent
                radius: 10
                insideColor: Qt.rgba(255, 255, 255, 0.05)
            }
        }
        onClicked: {
            Helper.workspace.createModel()
            Helper.workspace.switchTo(Helper.workspace.count - 1)
        }
    }

    Item {
        id: newWorkspaceDropArea
        anchors {
            left: workspaceList.right
            right: root.right
            top: root.top
            bottom: root.bottom
        }
        visible: Helper.workspace.count < TreelandConfig.maxWorkspace
        HoverHandler {
            onHoveredChanged: {
                if (hovered) {
                    if (dragManager.item?.wrapper && !dragManager.doNotRestoreAccept) {
                        dragManager.accept = () => {
                            const wid = Helper.workspace.createModel()
                            Helper.workspace.switchTo(Helper.workspace.count - 1)
                            Helper.workspace.moveSurfaceTo(dragManager.item.wrapper, wid)
                        }
                    }
                } else {
                    if (dragManager.item?.wrapper && !dragManager.doNotRestoreAccept)
                        dragManager.accept = null
                }
            }
        }
    }
}

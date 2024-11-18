// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Waylib.Server
import Treeland
import MultitaskView
import org.deepin.dtk as D
import org.deepin.dtk.style as DS

Multitaskview {
    id: root

    property D.Palette outerShadowColor: DS.Style.highlightPanel.dropShadow
    clip: true
    width: parent.width
    height: parent.height

    property bool initialized: false
    property bool exited: false
    readonly property QtObject taskViewGesture: Helper.multiTaskViewGesture
    property real taskviewVal: 0

    onStatusChanged: {
        if (root.status === Multitaskview.Exited)
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

        if (activeReason === Multitaskview.ShortcutKey) {
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
            duration: TreelandConfig.multitaskviewAnimationDuration
            property: "taskviewVal"
            easing.type: TreelandConfig.multitaskviewEasingCurveType
        }
    }

    QtObject {
        id: multitaskviewDragManager
        property Item item  // current dragged item
        property var accept // accept callback func
        property point destPoint
        property bool doNotRestoreAccept: false
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        preventStealing: true
        onClicked: {
            root.exit()
        }
    }

    Rectangle {
        id: multitaskviewBlackMask
        anchors.fill: parent
        color: "black"
        z: Multitaskview.Background
    }

    Repeater {
        model: Helper.rootContainer.outputModel
        Item {
            id: outputPlacementItem
            required property int index
            required property QtObject output
            x: output.outputItem.x
            y: output.outputItem.y
            width: output.outputItem.width
            height: output.outputItem.height

            WorkspaceSelectionList {
                id: workspaceSelectionList
                output: outputPlacementItem.output
                dragManager: multitaskviewDragManager
                multitaskview: root
            }

            Repeater {
                id: wsDelegates
                model: Helper.workspace.models
                WindowSelectionGrid {
                    id: windowSelectionGrid
                    width: parent.width
                    height: parent.height
                    visible: workspace.visible
                    multitaskview: root
                    output: outputPlacementItem.output
                    draggedParent: root
                    workspaceListPadding: TreelandConfig.workspaceDelegateHeight
                    dragManager: multitaskviewDragManager
                }
            }

            Connections {
                target: Helper.workspace.animationController
                function onRunningChanged() {
                    if (Helper.workspace.animationController.running)
                        workspaceAnimation.active = true
                }
            }

            Loader {
                id: workspaceAnimation
                active: false
                readonly property real localFactor: outputPlacementItem.width / Helper.workspace.animationController.refWidth
                anchors.fill: parent
                sourceComponent: Item {
                    id: animationDelegate
                    clip: true
                    visible: Helper.workspace.animationController.running
                    onVisibleChanged: {
                        if (!visible)
                            workspaceAnimation.active = false
                    }
                    Row {
                        spacing: Helper.workspace.animationController.refGap * workspaceAnimation.localFactor
                        x: -Helper.workspace.animationController.viewportPos * workspaceAnimation.localFactor
                        Repeater {
                            model: Helper.workspace.models
                            ShaderEffectSource {
                                required property int index
                                width: outputPlacementItem.width
                                height: outputPlacementItem.height
                                hideSource: visible
                                sourceItem: wsDelegates.itemAt(index) ?? null
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

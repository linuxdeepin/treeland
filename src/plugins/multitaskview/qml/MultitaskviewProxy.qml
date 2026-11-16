// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
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
    width: root.desktop.workspace.width
    height: root.desktop.workspace.height

    property bool initialized: false
    property bool exited: false
    property real taskviewVal: 0
    property bool inProgress: false

    readonly property bool enableAnimation: WaylandServer.animationConfig.multitaskview

    onActionFinished: {
        if (root.status === Multitaskview.Exited) {
            exited = true
            if (!root.enableAnimation) {
                taskviewVal = 0
                root.visible = false
            }
        } else {
            exited = false
        }

        if (root.activeReason === Multitaskview.Gesture) {
            inProgress = false
        }
    }

    onPartialFactorChanged: inProgress = true

    states: [
        State {
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
                taskviewVal: root.partialFactor
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
        if (!initialized || exited) return "initial";

        if (root.inProgress) return "partial";

        if (taskviewVal >= 0.5) return "taskview";

        return "initial";
    }

    transitions: [
        Transition {
            to: "initial"
            enabled: root.enableAnimation
            SequentialAnimation {
                NumberAnimation {
                    duration: root.desktop.config.multitaskviewAnimationDuration
                    property: "taskviewVal"
                    easing.type: root.desktop.config.multitaskviewEasingCurveType
                }
                ScriptAction {
                    script: root.visible = false;
                }
            }
        },
        Transition {
            to: "taskview"
            enabled: root.enableAnimation
            SequentialAnimation {
                NumberAnimation {
                    duration: root.desktop.config.multitaskviewAnimationDuration
                    property: "taskviewVal"
                    easing.type: root.desktop.config.multitaskviewEasingCurveType
                }
            }
        }
    ]

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
        model: WaylandServer.rootSurfaceContainer.outputModel
        Item {
            id: outputPlacementItem
            required property int index
            required property QtObject output
            x: output.outputItem.x
            y: output.outputItem.y
            width: output.outputItem.width
            height: output.outputItem.height

            Wallpaper {
                id: wallpaper
                output: outputPlacementItem.output.outputItem.output
                workspace: root.desktop.workspace.current
            }

            ShaderEffectSource {
                z: Multitaskview.Background
                sourceItem: wallpaper
                recursive: true
                live: true
                smooth: true
                anchors.fill: parent
                hideSource: visible
            }

            Blur {
                z: Multitaskview.Background
                anchors.fill: parent
                opacity: root.taskviewVal
                radiusEnabled: false
            }

            Item {
                id: validRectItem
                x: output.validRect.x
                y: output.validRect.y
                z: Multitaskview.Overlay
                width: output.validRect.width
                height: output.validRect.height
                WorkspaceSelectionList {
                    id: workspaceSelectionList
                    output: outputPlacementItem.output
                    dragManager: multitaskviewDragManager
                    multitaskview: root
                }

                Repeater {
                    id: wsDelegates
                    model: root.desktop.workspace.models
                    WindowSelectionGrid {
                        id: windowSelectionGrid
                        exited: root.exited
                        partialGestureFactor: root.taskviewVal
                        inProgress: !Number.isInteger(taskviewVal)
                        width: parent.width
                        height: parent.height
                        visible: workspace.visible
                        multitaskview: root
                        output: outputPlacementItem.output
                        draggedParent: root
                        workspaceListPadding: (root.desktop.config.workspaceThumbHeight + 2 * root.desktop.config.workspaceThumbMargin) / output.outputItem.devicePixelRatio
                        dragManager: multitaskviewDragManager
                    }
                }
            }

            Connections {
                target: root.desktop.workspace.animationController
                function onRunningChanged() {
                    if (root.desktop.workspace.animationController.running)
                        workspaceAnimation.active = true
                }
            }

            Loader {
                id: workspaceAnimation
                active: false
                readonly property real localFactor: outputPlacementItem.width / root.desktop.workspace.animationController.refWidth
                anchors.fill: parent
                sourceComponent: Item {
                    id: animationDelegate
                    clip: true
                    visible: root.desktop.workspace.animationController.running
                    onVisibleChanged: {
                        if (!visible)
                            workspaceAnimation.active = false
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: "black"
                    }
                    z: Multitaskview.Background
                    Row {
                        spacing: root.desktop.workspace.animationController.refGap * workspaceAnimation.localFactor
                        x: -root.desktop.workspace.animationController.viewportPos * workspaceAnimation.localFactor
                        Repeater {
                            model: root.desktop.workspace.models
                            Item {
                                required property int index
                                width: outputPlacementItem.output.outputItem.width
                                height: outputPlacementItem.output.outputItem.height
                                Wallpaper {
                                    id: wallpaper2
                                    output: outputPlacementItem.output.outputItem.output
                                    workspace: root.desktop.workspace.modelAt(index)
                                }

                                ShaderEffectSource {
                                    z: Multitaskview.Background
                                    sourceItem: wallpaper2
                                    recursive: true
                                    live: true
                                    smooth: true
                                    anchors.fill: parent
                                    hideSource: visible
                                }

                                Blur {
                                    z: Multitaskview.Background
                                    anchors.fill: parent
                                    opacity: root.taskviewVal
                                    radiusEnabled: false
                                }

                                ShaderEffectSource {
                                    x: outputPlacementItem.output.validRect.x
                                    y: outputPlacementItem.output.validRect.y
                                    width: outputPlacementItem.output.validRect.width
                                    height: outputPlacementItem.output.validRect.height
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

    Component.onCompleted: {
        initialized = true
    }
}

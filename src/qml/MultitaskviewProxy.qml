import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Treeland
import org.deepin.dtk as D
import org.deepin.dtk.style as DS

Multitaskview {
    id: root

    property D.Palette outerShadowColor: DS.Style.highlightPanel.dropShadow
    clip: true

    states: [
        State{
            name: "initial"
            PropertyChanges {
                root {
                    taskviewVal: 0
                }
            }
        },
        // State {
        //     name: "partial"
        //     PropertyChanges {
        //         target: root
        //         taskviewVal: taskViewGesture.partialGestureFactor
        //     }
        // },
        State {
            name: "taskview"
            PropertyChanges {
                root {
                    taskviewVal: 1
                }
            }
        }
    ]

    state: {
        if (status === Multitaskview.Uninitialized) return "initial";

        if (status === Multitaskview.Exited) {
            if (taskviewVal === 0)
                root.visible = false;

            return "initial";
        }

        if (activeReason === Multitaskview.ShortcutKey){
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
        id: dragManager
        property Item item  // current dragged item
        property var accept // accept callback func
        property point destPoint
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        preventStealing: true
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
            readonly property real whRatio: output.outputItem.width / output.outputItem.height
            x: output.outputItem.x
            y: output.outputItem.y
            width: output.outputItem.width
            height: output.outputItem.height
            z: 1

            Repeater {
                model: Helper.workspace.models
                SurfaceGridProxy {
                    id: grid
                    width: parent.width
                    height: parent.height
                    required property int index
                    required property WorkspaceModel modelData
                    workspace: modelData
                    visible: modelData.visible
                    taskviewVal: root.taskviewVal
                    state: root.state
                    output: outputPlacementItem.output
                    workspaceListPadding: TreelandConfig.workspaceDelegateHeight

                    delegate: Item {
                        id: surfaceItemDelegate
                        property bool shouldPadding: needPadding
                        property real ratio: wrapper.width / wrapper.height
                        property real fullY
                        D.BoxShadow {
                            anchors.fill: paddingRect
                            visible: shouldPadding
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

                        clip: false
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
                                        z: Multitaskview.FloatingItem
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

                        property bool highlighted: dragManager.item === null && (hvhdlr.hovered || surfaceCloseBtn.hovered)

                        // Item {
                        //     anchors.fill: parent
                        //     clip: shouldPadding
                        //     Item {
                        //         id: surfaceShot
                        //         width: parent.width
                        //         height: width / ratio
                        //         anchors.centerIn: parent

                        //         D.BoxShadow {
                        //             anchors.fill: parent
                        //             shadowColor: root.D.ColorSelector.outerShadowColor
                        //             shadowOffsetY: 2
                        //             shadowBlur: 10
                        //             cornerRadius: grid.delegateCornerRadius
                        //             hollow: true
                        //         }

                        //         ShaderEffectSource {
                        //             id: preview
                        //             anchors.fill: parent
                        //             live: true
                        //             // no hidesource, may conflict with workspace thumb
                        //             smooth: true
                        //             sourceItem: wrapper
                        //             visible: false
                        //         }

                        //         MultiEffect {
                        //             enabled: grid.delegateCornerRadius > 0
                        //             anchors.fill: preview
                        //             source: preview
                        //             maskEnabled: true
                        //             maskSource: mask
                        //         }

                        //         Item {
                        //             id: mask
                        //             anchors.fill: preview
                        //             layer.enabled: true
                        //             visible: false
                        //             Rectangle {
                        //                 anchors.fill: parent
                        //                 radius: grid.delegateCornerRadius
                        //             }
                        //         }
                        //     }
                        // }

                        SurfaceProxy {
                            id: surfaceProxy
                            surface: wrapper
                            live: true
                            fullProxy: true
                            width: parent.width
                            height: width / surfaceItemDelegate.ratio
                            anchors.centerIn: parent
                        }

                        Item {
                            z: 1
                            id: eventItem
                            anchors.fill: parent
                            HoverHandler {
                                id: hvhdlr
                                enabled: !drg.active
                            }
                            TapHandler {
                                gesturePolicy: TapHandler.WithinBounds
                                onTapped: root.exit(wrapper)
                            }
                            DragHandler {
                                id: drg
                                onActiveChanged: {
                                    if (active) {
                                        dragManager.item = surfaceItemDelegate
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
                                                       fullY = conv(workspacePreviewArea.height, workspacePreviewArea) - conv(mapToItem(surfaceItemDelegate, eventPoint.position).y, surfaceItemDelegate)
                                                       break
                                                   }
                                               }
                            }
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
                            visible: highlighted && wrapper.shellSurface.title !== ""

                            contentItem: Text {
                                text: wrapper.shellSurface.title
                                elide: Qt.ElideRight
                            }
                            background: Rectangle {
                                color: Qt.rgba(255, 255, 255, .2)
                                radius: TreelandConfig.titleBoxCornerRadius
                            }
                        }
                    }
                }
            }

            Item {
                id: workspacePreviewArea
                height: TreelandConfig.workspaceDelegateHeight
                width: parent.width
                z: Multitaskview.Overlay
                transform: [
                    Translate {
                        y: height * (taskviewVal - 1.0)
                    }
                ]
                ListView {
                    id: workspaceList
                    anchors {
                        top: parent.top
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
                                    Helper.workspace.count * (TreelandConfig.workspaceThumbHeight * outputPlacementItem.whRatio + 2 * TreelandConfig.workspaceThumbMargin))
                    orientation: ListView.Horizontal
                    height: TreelandConfig.workspaceDelegateHeight
                    model: Helper.workspace.models
                    delegate: WorkspaceThumbDelegate {
                        id: workspaceThumbDelegate
                        required property WorkspaceModel modelData
                        output: outputPlacementItem.output
                        workspace: modelData
                        workspaceManager: Helper.workspace
                        dm: dragManager
                        Drag.onActiveChanged: {
                            if (Drag.active) {
                                dragManager.item = this
                            } else {
                                if (dragManager.accept) {
                                    dragManager.accept()
                                }
                                dragManager.item = null
                            }
                        }
                        states: [
                            State {
                                name: "dragging"
                                when: workspaceThumbDelegate.Drag.active
                                PropertyChanges {
                                    restoreEntryValues: true
                                    workspaceThumbDelegate {
                                        parent: root
                                        z: Multitaskview.FloatingItem
                                        x: mapToItem(root, 0, 0).x
                                        y: mapToItem(root, 0, 0).y
                                    }
                                }
                            }
                        ]
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
                    icon.name: "list_add"
                    icon.height: height
                    icon.width: width
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
        }
    }
}

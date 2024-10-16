import QtQuick
import Treeland
import org.deepin.dtk as D
import org.deepin.dtk.style as DS

Item {
    id: root
    required property Workspace workspaceManager
    required property WorkspaceModel workspace
    required property QtObject dm // Drag manager
    required property int index
    required property QtObject output
    readonly property real whRatio: output.outputItem.width / output.outputItem.height
    property D.Palette outerShadowColor: DS.Style.highlightPanel.dropShadow

    height: TreelandConfig.workspaceDelegateHeight
    width: TreelandConfig.workspaceThumbHeight * root.whRatio + 2 * TreelandConfig.workspaceThumbMargin
    Drag.active: hdrg.active

    WorkspaceProxy {
        id: wp
        workspace: root.workspace
        output: root.output
    }

    DelegateModel.inPersistedItems: true
    property var initialState
    Rectangle {
        anchors {
            fill: parent
            margins: TreelandConfig.workspaceThumbMargin - TreelandConfig.highlightBorderWidth
        }
        border.width: workspace.visible ? TreelandConfig.highlightBorderWidth : 0
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

            ShaderEffectSource {
                sourceItem: wp
                sourceRect: Qt.rect(0,0,wp.width, wp.height)
                anchors.fill: parent
                hideSource: true
                recursive: true
            }

            HoverHandler {
                id: hvrhdlr
                enabled: !hdrg.active
                onHoveredChanged: {
                    // if (hovered) {
                    //     if (dragManager.item) {
                    //         if (dragManager.item.wrapper) {  // is dragging surface
                    //             if (workspace.index !== dragManager.item.wrapper.workspaceId) {
                    //                 //     dragManager.accept = () => {
                    //                 //         // TODO move surface to workspace model
                    //                 //     }
                    //             }
                    //         } else {    // is dragging workspace
                    //             // dragManager.destPoint = Qt.point(root.x, root.y)
                    //             // dragManager.accept = () => {
                    //             //     const draggedItem = dragManager.item
                    //             //     const draggedWs = QmlHelper.workspaceManager.workspacesById.get(draggedItem.wsid)
                    //             //     const destIndex = draggedItem.DelegateModel.itemsIndex
                    //             //     QmlHelper.workspaceManager.layoutOrder.move(draggedWs.workspaceRelativeId, destIndex, 1)
                    //             //     const newCurrentWorkspaceIndex = QmlHelper.workspaceManager.workspacesById.get(currentWsid).workspaceRelativeId
                    //             //     root.setCurrentWorkspaceId(newCurrentWorkspaceIndex)
                    //             //     draggedItem.x = dragManager.destPoint.x
                    //             //     draggedItem.y = dragManager.destPoint.y
                    //             // }
                    //             // visualModel.items.move(dragManager.item.DelegateModel.itemsIndex, root.DelegateModel.itemsIndex)
                    //         }
                    //     }
                    // } else {
                    //     if (dragManager.item?.wrapper) // is dragging surface, workspace always lose hover
                    //         dragManager.accept = null
                    // }
                }
            }

            TapHandler {
                id: taphdlr
                acceptedButtons: Qt.LeftButton
                enabled: !hdrg.active
                gesturePolicy: TapHandler.WithinBounds
                onTapped: {
                    if (workspace.index === workspaceManager.currentIndex) {
                        // TODO: exit multitaskview
                    } else {
                        workspaceManager.switchTo(index)
                    }
                }
            }

            TapHandler {
                id: quickHdlr
                acceptedButtons: Qt.RightButton
                enabled: !hdrg.active
                gesturePolicy: TapHandler.WithinBounds
                onTapped: {
                    workspaceManager.switchTo(index)
                    // TODO: exit multitaskview
                }
            }

            DragHandler {
                id: hdrg
                target: root
                yAxis.enabled: false
            }
        }

        D.ItemViewport {
            sourceItem: content
            radius: TreelandConfig.workspaceThumbCornerRadius
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
            visible: (workspaceManager.count > 1)
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
                workspaceManager.removeModel(workspace.index)
            }
        }
    }
}


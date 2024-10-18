import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Waylib.Server
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
            readonly property real whRatio: output.outputItem.width / output.outputItem.height
            x: output.outputItem.x
            y: output.outputItem.y
            width: output.outputItem.width
            height: output.outputItem.height
            WallpaperController {
                id: wallpaperController
                output: outputPlacementItem.output.outputItem.output
                lock: true
                type: WallpaperController.Normal
            }

            ShaderEffectSource {
                sourceItem: wallpaperController.proxy
                recursive: true
                live: true
                smooth: true
                anchors.fill: parent
                hideSource: false
            }

            RenderBufferBlitter {
                z: Multitaskview.Background
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
                model: Helper.workspace.models
                SurfaceGridProxy {
                    id: grid
                    width: parent.width
                    height: parent.height
                    required property int index
                    visible: workspace.visible
                    state: root.state
                    output: outputPlacementItem.output
                    workspaceListPadding: TreelandConfig.workspaceDelegateHeight
                    onRequestExit: function(surface){
                        root.exit(surface)
                    }
                    delegate: Item {
                        id: surfaceItemDelegate
                        property bool shouldPadding: needPadding
                        property real ratio: wrapper.width / wrapper.height
                        property real fullY
                        D.BoxShadow {
                            anchors.fill: paddingRect
                            visible: shouldPadding
                            cornerRadius: grid.delegateCornerRadius
                            shadowColor: Qt.rgba(0, 0, 0, 0.3)
                            shadowOffsetY: 16
                            shadowBlur: 32
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
                        property bool highlighted: dragManager.item === null && (hvhdlr.hovered || surfaceCloseBtn.hovered) && root.state === "taskview"
                        SurfaceProxy {
                            enabled: false
                            id: surfaceProxy
                            surface: wrapper
                            live: true
                            fullProxy: true
                            width: parent.width
                            height: width / surfaceItemDelegate.ratio
                            anchors.centerIn: parent
                        }
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

                Item {
                    id: animationMask
                    property real localAnimationFactor: (TreelandConfig.workspaceThumbHeight * outputPlacementItem.whRatio+ 2 * TreelandConfig.workspaceThumbMargin)
                                                        / Helper.workspace.animationController.refWrap
                    visible: Helper.workspace.animationController.running
                    anchors.fill: workspaceList
                    anchors.margins: TreelandConfig.workspaceThumbMargin - TreelandConfig.highlightBorderWidth
                    Rectangle {
                        width: TreelandConfig.workspaceThumbHeight * outputPlacementItem.whRatio + 2 * TreelandConfig.highlightBorderWidth
                        height: TreelandConfig.workspaceThumbHeight + 2 * TreelandConfig.highlightBorderWidth
                        border.width: TreelandConfig.highlightBorderWidth
                        border.color: "blue"
                        color: "transparent"
                        radius: TreelandConfig.workspaceThumbCornerRadius + TreelandConfig.highlightBorderWidth
                        x: Helper.workspace.animationController.viewportPos * animationMask.localAnimationFactor
                    }
                }
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
                        onRequestExit: {
                            root.exit()
                        }

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

            Loader {
                id: workspaceAnimationLoader
                active: false
                Connections {
                    target: Helper.workspace.animationController
                    function onRunningChanged() {
                        if (Helper.workspace.animationController.running) workspaceAnimationLoader.active = true
                    }
                }
                anchors.fill: parent
                sourceComponent: Item {
                    id: animationDelegate
                    visible: Helper.workspace.animationController.running
                    onVisibleChanged: {
                        if (!visible) {
                            workspaceAnimationLoader.active = false
                        }
                    }
                    property real localFactor: width / Helper.workspace.animationController.refWidth

                    Rectangle {
                        anchors.fill: parent
                        color: "black"
                    }
                    Row {
                        visible: true
                        spacing: Helper.workspace.animationController.refGap * animationDelegate.localFactor
                        x: -Helper.workspace.animationController.viewportPos * animationDelegate.localFactor
                        Repeater {
                            model: Helper.workspace.models
                            Item {
                                id: wsShot
                                required property int index
                                width: animationDelegate.width
                                height: animationDelegate.height
                                ShaderEffectSource {
                                    z: Multitaskview.Background
                                    id: wallpaperShot
                                    live: true
                                    smooth: true
                                    sourceItem: wallpaperController.proxy
                                    anchors.fill: parent
                                }

                                RenderBufferBlitter {
                                    z: Multitaskview.Background
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

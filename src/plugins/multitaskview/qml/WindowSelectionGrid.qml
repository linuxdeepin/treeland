// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland
import MultitaskView
import Waylib.Server
import QtQuick.Effects
import QtQuick.Controls
import org.deepin.dtk as D
import org.deepin.dtk.style as DS

Item {
    id: root
    required property QtObject output
    required property WorkspaceModel workspace
    required property int workspaceListPadding
    required property Item draggedParent
    required property QtObject dragManager
    required property Multitaskview multitaskview
    required property bool exited
    required property real partialGestureFactor
    required property bool inProgress

    readonly property real delegateCornerRadius: (ros.rows >= 1 && ros.rows <= 3) ? ros.cornerRadiusList[ros.rows - 1] : ros.cornerRadiusList[2]

    property int loadedWindowCount: 0
    property bool loaderFinished: windowLoader.status === Loader.Ready
    focus: true
    onVisibleChanged: {
        if (visible)
            forceActiveFocus()
    }

    QtObject {
        id: ros // readonly state
        property list<real> cornerRadiusList: [18,12,8] // Should get from system preference
        readonly property int rows: surfaceModel.rows
    }

    MultitaskviewSurfaceModel {
        id: surfaceModel
        workspace: root.workspace
        output: root.output
        layoutArea: root.mapToItem(output.outputItem, Qt.rect(surfaceGridView.x, surfaceGridView.y, surfaceGridView.width, surfaceGridView.height))
    }

    Connections {
        target: multitaskview
        function onAboutToExit() {
            surfaceModel.updateZOrder()
        }
    }

    Flickable {
        id: surfaceGridView
        // Do not use anchors here, geometry should be stable as soon as the item is created
        y: workspaceListPadding
        contentHeight: Math.max(height, surfaceModel.contentHeight)
        width: parent.width
        height: parent.height - workspaceListPadding
        states: [
            State {
                name: "initial"
                PropertyChanges {
                    surfaceGridView {
                        clip: false
                    }
                }
            },
            State {
                name: "partial"
                PropertyChanges {
                    surfaceGridView {
                        clip: false
                    }
                }
            },
            State {
                name: "taskview"
                PropertyChanges {
                    surfaceGridView {
                        clip: true
                    }
                }
            }
        ]
        transitions: [
            Transition {
                to: "taskview"
                SequentialAnimation {
                    PauseAnimation { duration: Helper.config.multitaskviewAnimationDuration }
                    PropertyAction { target: surfaceGridView; property: "clip" }
                }
            }
        ]
        state: {
            if (exited || status === Multitaskview.Uninitialized)
                return "initial";

            if (multitaskview.activeReason === Multitaskview.ShortcutKey){
                return "taskview";
            } else {
                if (root.inProgress) return "partial";

                if (root.partialGestureFactor > 0.5) return "taskview";
            }
            return "initial";
        }
        visible: surfaceModel.modelReady

        Loader {
            id: windowLoader
            sourceComponent: Repeater {
                id: surfaceRepeater
                model: surfaceModel
                Item {
                    id: surfaceItemDelegate

                    required property int index
                    required property SurfaceWrapper wrapper
                    required property rect geometry
                    required property bool padding
                    required property int zorder
                    required property bool minimized
                    required property int upIndex
                    required property int downIndex
                    required property int leftIndex
                    required property int rightIndex

                    property bool needPadding
                    property real paddingOpacity
                    readonly property rect initialGeometry: surfaceGridView.mapFromItem(multitaskview, wrapper.geometry)
                    property real ratio: wrapper.width / wrapper.height
                    property real fullY

                    focus: true
                    visible: multitaskview.status == Multitaskview.Exited ? !minimized : true
                    clip: false
                    state:  {
                        drg.active ? "dragging" : surfaceGridView.state
                    }
                    x: geometry.x
                    y: geometry.y
                    z: zorder
                    width: geometry.width
                    height: geometry.height
                    states: [
                        State {
                            name: "initial"
                            PropertyChanges {
                                surfaceItemDelegate {
                                    x: initialGeometry.x
                                    y: initialGeometry.y
                                    width: initialGeometry.width
                                    height: initialGeometry.height
                                    scale: 1.0
                                    paddingOpacity: 0
                                    needPadding: false
                                }
                            }
                        },
                        State {
                            name: "partial"
                            PropertyChanges {
                                surfaceItemDelegate {
                                    x: (geometry.x - initialGeometry.x) * multitaskview.taskviewVal + initialGeometry.x
                                    y: (geometry.y - initialGeometry.y) * multitaskview.taskviewVal + initialGeometry.y
                                    width: (geometry.width - initialGeometry.width) * multitaskview.taskviewVal + initialGeometry.width
                                    height: (geometry.height - initialGeometry.height) * multitaskview.taskviewVal + initialGeometry.height
                                    scale: 1.0
                                    paddingOpacity: Helper.config.multitaskviewPaddingOpacity * multitaskview.taskviewVal
                                    needPadding: true
                                }
                            }
                        },
                        State {
                            name: "taskview"
                            PropertyChanges {
                                surfaceItemDelegate {
                                    x: geometry.x
                                    y: geometry.y
                                    width: geometry.width
                                    height: geometry.height
                                    needPadding: padding
                                    scale: 1.0
                                    paddingOpacity: Helper.config.multitaskviewPaddingOpacity
                                }
                            }
                        },
                        State {
                            name: "dragging"
                            PropertyChanges {
                                // restoreEntryValues: true // FIXME: does this restore propery binding?
                                surfaceItemDelegate {
                                    parent: draggedParent
                                    x: mapToItem(draggedParent, 0, 0).x
                                    y: mapToItem(draggedParent, 0, 0).y
                                    z: Multitaskview.FloatingItem
                                    scale: (Math.max(0, Math.min(drg.activeTranslation.y / fullY, 1)) * (100 - width) + width) / width
                                    paddingOpacity: Helper.config.multitaskviewPaddingOpacity
                                    transformOrigin: Item.Center
                                    needPadding: false
                                }
                            }
                        }
                    ]

                    Behavior on x {enabled:state !== "dragging" && state !== "partial"; XAnimator {duration: Helper.config.multitaskviewAnimationDuration; easing.type: Helper.config.multitaskviewEasingCurveType} }
                    Behavior on y {enabled:state !== "dragging" && state !== "partial"; YAnimator {duration: Helper.config.multitaskviewAnimationDuration; easing.type: Helper.config.multitaskviewEasingCurveType} }
                    Behavior on width {enabled:state !== "dragging" && state !== "partial"; NumberAnimation {duration: Helper.config.multitaskviewAnimationDuration; easing.type: Helper.config.multitaskviewEasingCurveType} }
                    Behavior on height {enabled:state !== "dragging" && state !== "partial"; NumberAnimation {duration: Helper.config.multitaskviewAnimationDuration; easing.type: Helper.config.multitaskviewEasingCurveType} }
                    Behavior on paddingOpacity {enabled:state !== "dragging" && state !== "partial"; NumberAnimation {duration: Helper.config.multitaskviewAnimationDuration; easing.type: Helper.config.multitaskviewEasingCurveType} }
                    Behavior on scale {enabled:state !== "dragging" && state !== "partial"; ScaleAnimator {duration: Helper.config.multitaskviewAnimationDuration; easing.type: Helper.config.multitaskviewEasingCurveType} }

                    D.BoxShadow {
                        id: paddingRectShadow
                        anchors.fill: paddingRect
                        visible: needPadding
                        cornerRadius: delegateCornerRadius
                        shadowColor: Qt.rgba(0, 0, 0, 0.3)
                        shadowOffsetY: 16
                        shadowBlur: 32
                        hollow: true
                    }

                    Rectangle {
                        id: paddingRect
                        visible: needPadding
                        radius: delegateCornerRadius
                        anchors.fill: parent
                        color: "white"
                        opacity: paddingOpacity
                    }

                    Rectangle {
                        border.color: "blue"
                        border.width: Helper.config.highlightBorderWidth
                        visible: highlighted
                        anchors.margins: -Helper.config.highlightBorderWidth
                        anchors.fill: parent
                        color: "transparent"
                        radius: delegateCornerRadius + Helper.config.highlightBorderWidth
                    }

                    function conv(y, item = parent) { // convert to draggedParent's coord
                        return mapToItem(draggedParent, mapFromItem(item, 0, y)).y
                    }

                    property bool hovered: hvhdlr.hovered || surfaceCloseBtn.hovered
                    onHoveredChanged: {
                        if (hovered) {
                            surfaceItemDelegate.forceActiveFocus()
                        } else {
                            root.forceActiveFocus()
                        }
                    }

                    property bool highlighted: dragManager.item === null && activeFocus && surfaceItemDelegate.state === "taskview"
                    SurfaceProxy {
                        id: surfaceProxy
                        surface: surfaceItemDelegate.wrapper
                        live: true
                        fullProxy: true
                        radius: delegateCornerRadius
                        width: parent.width
                        height: width / surfaceItemDelegate.ratio
                        anchors.centerIn: parent
                    }
                    HoverHandler {
                        id: hvhdlr
                        enabled: !drg.active
                        blocking: true
                    }
                    TapHandler {
                        gesturePolicy: TapHandler.WithinBounds
                        onTapped: {
                            surfaceModel.updateZOrder()
                            multitaskview.exit(wrapper)
                        }
                    }
                    DragHandler {
                        id: drg
                        onActiveChanged: {
                            if (active) {
                                dragManager.item = surfaceItemDelegate
                            } else {
                                dragManager.doNotRestoreAccept = false
                                if (dragManager.accept) {
                                    dragManager.accept()
                                }
                                dragManager.item = null
                            }
                        }
                        onGrabChanged: (transition, eventPoint) => {
                                           switch (transition) {
                                               case PointerDevice.GrabExclusive:
                                               fullY = conv(workspaceListPadding, root) - conv(mapToItem(surfaceItemDelegate, eventPoint.position).y, surfaceItemDelegate)
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
                        focusPolicy: Qt.NoFocus // Should not steal focus from delegate
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
                            surfaceItemDelegate.wrapper.requestClose()
                        }
                    }
                    Keys.onPressed: function (event) {
                        if (event.modifiers === Qt.NoModifier) {
                            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                                multitaskview.exit(wrapper)
                            } else if (event.key === Qt.Key_Delete) {
                                wrapper.requestClose()
                            }
                        } else if (event.modifiers === (Qt.MetaModifier | Qt.ShiftModifier)) {
                            const workspaceNumKeys = [Qt.Key_1, Qt.Key_2, Qt.Key_3, Qt.Key_4, Qt.Key_5, Qt.Key_6]
                            const workspaceNumComposedKeys = [Qt.Key_Exclam, Qt.Key_At, Qt.Key_NumberSign, Qt.Key_Dollar, Qt.Key_Percent, Qt.Key_AsciiCircum]
                            if (workspaceNumKeys.includes(event.key)
                                    || workspaceNumComposedKeys.includes(event.key)) {
                                let index = workspaceNumKeys.includes(event.key) ?
                                        workspaceNumKeys.indexOf(event.key) : workspaceNumComposedKeys.indexOf(event.key)
                                const ws = Helper.workspace.modelAt(index)
                                if (ws && wrapper)
                                    Helper.workspace.moveSurfaceTo(wrapper, ws.id)
                            }
                        } else if (event.modifiers === Qt.AltModifier
                                   && event.key === Qt.Key_QuoteLeft && loaderFinished) {
                            surfaceRepeater.itemAt(surfaceModel.nextSameAppIndex(index)).forceActiveFocus()
                        } else if (event.modifiers === (Qt.AltModifier | Qt.ShiftModifier)
                                   && event.key === Qt.Key_AsciiTilde && loaderFinished) {
                            surfaceRepeater.itemAt(surfaceModel.prevSameAppIndex(index)).forceActiveFocus()
                        }
                    }
                    KeyNavigation.right: (loaderFinished && loadedWindowCount > 0) ? surfaceRepeater.itemAt(rightIndex) : null
                    KeyNavigation.left: (loaderFinished && loadedWindowCount > 0) ? surfaceRepeater.itemAt(leftIndex) : null
                    KeyNavigation.up: (loaderFinished && loadedWindowCount > 0) ? surfaceRepeater.itemAt(upIndex) : null
                    KeyNavigation.down: (loaderFinished && loadedWindowCount > 0) ? surfaceRepeater.itemAt(downIndex) : null
                    KeyNavigation.tab: (loaderFinished && loadedWindowCount > 0) ? surfaceRepeater.itemAt((index + 1) % surfaceRepeater.count) : null
                    KeyNavigation.backtab: (loaderFinished && loadedWindowCount > 0) ? surfaceRepeater.itemAt((index + surfaceRepeater.count - 1) % surfaceRepeater.count) : null

                    Control {
                        id: titleBox
                        anchors {
                            bottom: parent.bottom
                            horizontalCenter: parent.horizontalCenter
                            margins: 10
                        }
                        width: Math.min(implicitContentWidth + 2 * padding, parent.width)
                        padding: 10
                        visible: surfaceItemDelegate.state === "taskview" && wrapper.shellSurface.title !== ""

                        contentItem: Text {
                            text: wrapper.shellSurface.title
                            elide: Qt.ElideRight
                        }
                        background: Item {
                            Rectangle {
                                id: bgRect
                                anchors.fill: parent
                                color: Qt.rgba(16, 16, 16, .1)
                                radius: Helper.config.titleBoxCornerRadius
                            }
                            Blur {
                                anchors.fill: bgRect
                                radius: Helper.config.titleBoxCornerRadius
                            }
                            Border {
                                anchors.fill: parent
                                radius: Helper.config.titleBoxCornerRadius
                                insideColor: Qt.rgba(255, 255, 255, 0.05)
                            }
                        }
                    }
                }
                onItemAdded: loadedWindowCount++
                onItemRemoved: loadedWindowCount--
            }
        }

        Loader {
            active: surfaceModel.count === 0
            anchors.centerIn: parent
            sourceComponent: Control {
                id: emptyWindowHint
                width: 200
                height: 50
                padding: 10
                Behavior on opacity {
                    enabled: state !== "partial"
                    OpacityAnimator {
                        duration: Helper.config.multitaskviewAnimationDuration
                        easing.type: Helper.config.multitaskviewEasingCurveType
                    }
                }
                state: multitaskview.state
                states: [
                    State {
                        name: "initial"
                        PropertyChanges {
                            emptyWindowHint {
                                opacity: 0
                            }
                        }
                    },
                    State {
                        name: "partial"
                        PropertyChanges {
                            emptyWindowHint {
                                opacity: multitaskview.taskviewVal
                            }
                        }
                    },
                    State {
                        name: "taskview"
                        PropertyChanges {
                            emptyWindowHint {
                                opacity: 1
                            }
                        }
                    }
                ]
                opacity: multitaskview.taskviewVal
                contentItem: Text {
                    text: qsTr("No windows")
                    font.pixelSize: 20
                    elide: Qt.ElideRight
                    anchors.fill: parent
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                }
                background: Item {
                    Rectangle {
                        id: hintBgRect
                        anchors.fill: parent
                        color: Qt.rgba(16, 16, 16, .1)
                        radius: Helper.config.titleBoxCornerRadius
                    }
                    Blur {
                        anchors.fill: hintBgRect
                        radius: Helper.config.titleBoxCornerRadius
                    }
                    D.BoxShadow {
                        anchors.fill: parent
                        visible: true
                        cornerRadius: Helper.config.titleBoxCornerRadius
                        shadowColor: Qt.rgba(0, 0, 0, 0.15)
                        shadowOffsetY: 8
                        shadowBlur: 20
                        hollow: true
                    }
                    Border {
                        anchors.fill: parent
                        radius: Helper.config.titleBoxCornerRadius
                        insideColor: Qt.rgba(255, 255, 255, 0.05)
                    }
                }
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: {
                multitaskview.exit()
            }
        }
    }
    Keys.onPressed: function (event) {
        if (event.modifiers === Qt.NoModifier) {
            if (event.key === Qt.Key_Home) {
                windowLoader.item.itemAt(0)?.forceActiveFocus()
            } else if (event.key === Qt.Key_End) {
                windowLoader.item.itemAt(windowLoader.item.count - 1)?.forceActiveFocus()
            } else if (event.key === Qt.Key_Escape) {
                multitaskview.exit()
            }
        } else if (event.modifiers === Qt.AltModifier) {
            if (event.key === Qt.Key_Minus && Helper.workspace.count > 1) {
                Helper.workspace.removeModel(Helper.workspace.currentIndex)
            } else if (event.key === Qt.Key_Equal && Helper.workspace.count < Helper.config.maxWorkspace) {
                Helper.workspace.createModel()
                Helper.workspace.switchTo(Helper.workspace.count - 1)
            }
        }
    }
    KeyNavigation.tab: (loaderFinished && loadedWindowCount > 0) ? windowLoader.item.itemAt(0) : null
    KeyNavigation.backtab: (loaderFinished && loadedWindowCount > 0) ? windowLoader.item.itemAt(loadedWindowCount - 1) : null
    KeyNavigation.right: (loaderFinished && loadedWindowCount > 0) ? windowLoader.item.itemAt(0) : null
    KeyNavigation.left: (loaderFinished && loadedWindowCount > 0) ? windowLoader.item.itemAt(loadedWindowCount - 1) : null
    KeyNavigation.down: (loaderFinished && loadedWindowCount > 0) ? windowLoader.item.itemAt(0) : null
    KeyNavigation.up:  (loaderFinished && loadedWindowCount > 0) ? windowLoader.item.itemAt(loadedWindowCount - 1) : null

    Component.onCompleted: {
        surfaceModel.calcLayout()
        forceActiveFocus()
    }
}

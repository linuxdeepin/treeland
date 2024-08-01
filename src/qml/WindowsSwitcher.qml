// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import Waylib.Server
import TreeLand
import QtQuick.Effects
import org.deepin.dtk 1.0 as D

ColumnLayout {
    id: root

    property bool showAllSurface: false
    property alias model: switchView.model
    required property OutputDelegate activeOutput
    // control all switch item
    property bool enableBlur: GraphicsInfo.api !== GraphicsInfo.Software
    property bool enableBorders: true
    property bool enableShadows: GraphicsInfo.api !== GraphicsInfo.Software
    property bool enableRadius: true
    property bool enableAnimation: GraphicsInfo.api !== GraphicsInfo.Software
    property bool stopWork: true
    readonly property int leftpreferredMargin: 20
    readonly property int rightpreferredMargin: 20
    readonly property real radius: enableRadius ? 18 : 0

    signal surfaceActivated(surface: SurfaceWrapper)

    spacing: 30

    onVisibleChanged: {
        if (visible) {
            root.model = workspaceManager.workspacesById.get(workspaceManager.layoutOrder.get(currentWorkspaceId).wsid).surfaces
            root.showAllSurface = false
            previewReductionAni.stop()
            preContext.sourceComponent = undefined
            currentContext.sourceComponent = undefined
            currentContext.loaderStatus = 0

            if (!switchView.model)
                return

            const surfaceItem = switchView.model.get(switchView.currentIndex).item
            if (!surfaceItem)
                return

            preContext.sourceSueface = null
            currentContext.sourceSueface = surfaceItem
            switchView.currentIndex = 0
            root.stopWork = false
        }
    }

    function ensurePreView() {
        if (!preContext.sourceComponent && root.enableAnimation)
            preContext.sourceComponent = preContext.previewComponent

        if (root.enableAnimation) {
            preContext.loaderStatus = 1
            preContext.loaderStatus = 0
        }

        if (!currentContext.sourceComponent)
            currentContext.sourceComponent = currentContext.previewComponent

        if (root.enableAnimation) {
            currentContext.loaderStatus = 0
            currentContext.loaderStatus = 1
        }
    }

    function previous() {
        if (switchView.count === 1)
            return;

        if (switchView.currentIndex === 0) {
            switchView.currentIndex = switchView.count - 1;
            return
        }
        switchView.currentIndex = switchView.currentIndex - 1
    }

    function next() {
        if (switchView.count === 1)
            return;

        if (switchView.currentIndex === switchView.count - 1) {
            switchView.currentIndex = 0
            return
        }
        switchView.currentIndex = switchView.currentIndex + 1
    }

    function activeAllSurfaces() {
        const wrapper = model.get(switchView.currentIndex).wrapper
        if (wrapper) {
            surfaceActivated(wrapper)
        }
    }

    function handleExit() {
        if (!root.visible)
            return

        if (currentContext.sourceComponent) {
            if (root.enableAnimation) {
                switchViewContentAni.stop()
                switchViewContentAni.isInAni = false
                switchViewContentAni.start()
            }

            if (root.enableAnimation) {
                root.stopWork = true
                preContext.sourceComponent = undefined
                currentContext.item.transformOrigin = Item.TopLeft
                previewReductionAni.target = currentContext.item
                previewReductionAni.scaleTo = currentContext.sourceSueface.width / currentContext.width
                currentContext.item.scale = 1.0
                currentContext.item.x = 0
                currentContext.item.y = 0
                previewReductionAni.xFrom = 0
                previewReductionAni.yFrom = 0
                var point = currentContext.item.mapFromItem(root.model.get(switchView.currentIndex).item, 0, 0)
                previewReductionAni.xTo = point.x
                previewReductionAni.yTo = point.y
                previewReductionAni.start()
                activeAllSurfaces()
            } else {
                root.visible = false
            }
        } else {
            root.visible = false
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: leftpreferredMargin
        Layout.rightMargin: rightpreferredMargin

        WindowPreviewLoader {
            id: preContext

            anchors.centerIn: parent
            sourceComponent: undefined
            onClicked: handleExit()
        }

        WindowPreviewLoader {
            id: currentContext

            anchors.centerIn: parent
            sourceComponent: undefined
        }

        ParallelAnimation {
            id: previewReductionAni

            readonly property int duration: 400
            property real scaleTo
            property real xFrom
            property real yFrom
            property real xTo
            property real yTo
            property Item target

            ScaleAnimator {
                target: previewReductionAni.target;
                from: 1.0;
                to: previewReductionAni.scaleTo
                duration: previewReductionAni.duration
                easing.type: Easing.OutExpo
            }

            XAnimator {
                target: previewReductionAni.target;
                from: previewReductionAni.xFrom
                to: previewReductionAni.xTo
                duration: previewReductionAni.duration
                easing.type: Easing.OutExpo
            }

            YAnimator {
                target: previewReductionAni.target;
                from: previewReductionAni.yFrom
                to: previewReductionAni.yTo
                duration: previewReductionAni.duration
                easing.type: Easing.OutExpo
            }

            onFinished:  {
                root.showAllSurface = true
                root.visible = false
                previewReductionAni.target.scale = 1.0
            }
        }
    }

    Item {
        id: switchViewContent

        Layout.preferredHeight: 198
        Layout.preferredWidth: switchView.contentWidth + 2 * switchView.vSpacing
        Layout.maximumWidth: parent.width
        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
        Layout.leftMargin: switchView.contentWidth > root.width ? 0 : leftpreferredMargin
        Layout.rightMargin: switchView.contentWidth > root.width ? 0 : rightpreferredMargin

        Loader {
            enabled: root.enableShadows
            anchors.fill: parent
            sourceComponent: root.enableShadows ? shadow : undefined
        }

        Component {
            id: shadow

            D.BoxShadow {
                shadowColor: "#000000"
                opacity: 0.2
                shadowOffsetY: 2
                shadowBlur: 8
                cornerRadius: root.radius
                hollow: true
            }
        }

        Rectangle {
            visible: root.enableBorders
            anchors {
                fill: parent
                margins: 1
            }
            color: "transparent"
            opacity: 0.1
            radius: root.radius
            border {
                color: "#000000"
                width: 1
            }
        }

        RenderBufferBlitter {
            id: blitter

            anchors.fill: parent
            visible: root.enableBlur

            MultiEffect {
                id: blur

                anchors.fill: parent
                source: blitter.content
                autoPaddingEnabled: false
                blurEnabled: root.enableBlur
                blur: 1.0
                blurMax: 64
                saturation: 0.2
            }

            D.ItemViewport {
                anchors.fill: blur
                fixed: true
                sourceItem: blur
                radius: root.radius
                hideSource: true
            }
        }

        Rectangle {
            visible: root.enableBorders
            anchors.fill: parent
            color: "transparent"
            opacity: 0.1
            radius: root.radius
            border {
                color: "#000000"
                width: 1
            }
        }

        ListView {
            id: switchView

            readonly property real titleheight: 40
            readonly property real fullheight: switchView.height
            readonly property real vMargin: 40
            readonly property real thumbnailheight: fullheight - titleheight
            readonly property real titleHMargin: 7
            readonly property real borderMargin: 4
            readonly property real vSpacing: 8
            readonly property size iconSize: Qt.size(24, 24)
            readonly property real borderWidth: borderMargin
            readonly property real delegateMinWidth: 80
            readonly property real delegateMaxWidth: 260
            readonly property real separatorHeight: 1
            readonly property real radius: root.enableRadius ? 12 : 0

            // control listview delegate and highlight
            property bool enableDelegateBlur: root.enableBlur
            property bool enableDelegateBorders: root.enableBorders
            property bool enableDelegateShadows: root.enableShadows
            property bool enableDelegateAnimation: root.enableAnimation

            orientation: ListView.Horizontal
            clip: true
            anchors {
                fill: parent
                leftMargin: vSpacing
                rightMargin: vSpacing
            }
            delegate: WindowThumbnailDelegate {}
            highlight: SwitcherThumbnailHighlightDelegate {}
            highlightFollowsCurrentItem: false
            currentIndex: 0

            onCurrentIndexChanged: {
                if (root.stopWork)
                    return

                switchView.positionViewAtIndex(currentIndex, ListView.Center)

                if (!switchView.model)
                    return

                if (currentIndex > 0) {
                    var preInidex

                    if (switchView.count === 1) {
                        preInidex = -1;
                    } else if (switchView.currentIndex === 0) {
                        preInidex = switchView.count - 1;
                    } else {
                        preInidex = currentIndex - 1
                    }

                    if (preInidex > -1) {
                        const preSurfaceItem = switchView.model.get(preInidex).item
                        if (preSurfaceItem) {
                            preContext.sourceSueface = preSurfaceItem
                        }
                    }
                }

                const surfaceItem = switchView.model.get(currentIndex).item
                if (!surfaceItem)
                    return

                currentContext.sourceSueface = surfaceItem
                ensurePreView()
            }
        }

        onVisibleChanged: {
            if (root.visible && root.enableAnimation) {
                switchViewContentAni.fromeY = switchViewContent.y
                switchViewContentAni.stop()
                switchViewContentAni.isInAni = true
                switchViewContentAni.start()
            }
        }
    }

    ParallelAnimation {
        id: switchViewContentAni

        property bool isInAni: true
        readonly property int duration: 400
        readonly property real toY: root.height - 60
        property real fromeY

        YAnimator {
            target: switchViewContent
            from: switchViewContentAni.isInAni ? switchViewContentAni.toY : switchViewContentAni.fromeY
            to: switchViewContentAni.isInAni ? switchViewContentAni.fromeY : switchViewContentAni.toY
            duration: switchViewContentAni.duration
            easing.type: Easing.OutExpo
        }

        OpacityAnimator {
            target: switchViewContent
            from: switchViewContentAni.isInAni ? 0.0 : 1.0
            to: switchViewContentAni.isInAni ? 1.0 : 0.0
            duration: switchViewContentAni.duration
            easing.type: Easing.OutExpo
        }

        onStopped: switchViewContent.y = switchViewContentAni.fromeY
    }
}

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

    property alias model: switchView.model
    required property OutputDelegate activeOutput
    // control all switch item
    property bool enableBlur: GraphicsInfo.api !== GraphicsInfo.Software
    property bool enableBorders: true
    property bool enableShadows: GraphicsInfo.api !== GraphicsInfo.Software
    property bool enableRadius: true
    property bool enableAnimation: GraphicsInfo.api !== GraphicsInfo.Software
    property bool stopWork: true
    property SurfaceWrapper activeSurface: null
    readonly property int leftpreferredMargin: 20
    readonly property int rightpreferredMargin: 20
    readonly property real radius: enableRadius ? 18 : 0
    readonly property int windowsExpandLimit: 18

    signal surfaceActivated(surface: SurfaceWrapper)
    signal previewClicked()

    spacing: 30

    onVisibleChanged: {
        if (visible) {
            root.model = workspaceManager.workspacesById.get(workspaceManager.layoutOrder.get(currentWorkspaceId).wsid).surfaces
            hideAllSurfaces()
            previewReductionAni.stop()
            preContext.sourceComponent = undefined
            currentContext.sourceComponent = undefined
            currentContext.loaderStatus = 0

            if (!switchView.model)
                return

            const surfaceItem = switchView.model.get(switchView.currentIndex).item
            if (!surfaceItem)
                return

            currentContext.sourceSueface = surfaceItem
            currentContext.cornerRadius = switchView.model.get(switchView.currentIndex).wrapper.cornerRadius
            switchView.currentIndex = 0
            root.stopWork = false
            ensurePreView()
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
        if (switchView.count === 1) {
            switchView.currentIndex = 0
            return
        }

        if (switchView.currentIndex === 0) {
            switchView.currentIndex = switchView.count - 1
            return
        }
        switchView.currentIndex = switchView.currentIndex - 1
    }

    function next() {
        if (switchView.count === 1) {
            switchView.currentIndex = 0
            return
        }

        if (switchView.currentIndex === switchView.count - 1) {
            switchView.currentIndex = 0
            return
        }
        switchView.currentIndex = switchView.currentIndex + 1
    }

    function activeCurrentSurface(enableAni) {
        if (root.activeSurface) {
            surfaceActivated(root.activeSurface)
            if (enableAni) {
                root.activeSurface.surfaceItem.opacity = 0
            }
        }
    }

    function hideAllSurfaces() {
        var sourceWrapperItem
        var sourceSuefaceItem
        for (var i = 0; i < switchView.count; ++i) {
            sourceWrapperItem = root.model.get(i).wrapper
            if (root.enableAnimation) {
                if (sourceWrapperItem.isMinimized)
                    continue

                if (sourceWrapperItem) {
                    sourceWrapperItem.scaleStatus = -1
                    sourceWrapperItem.inHomeAni = false
                }
            }

            if (sourceWrapperItem.isMinimized)
                continue

            sourceSuefaceItem = sourceWrapperItem.surfaceItem
            if (sourceSuefaceItem) {
                sourceSuefaceItem.opacity = 0.0
            }
        }
    }

    function showAllInactiveSurfaces() {
        var sourceSuefaceItem
        var sourceWrapperItem
        for (var i = 0; i < switchView.count; ++i) {
            sourceWrapperItem = root.model.get(i).wrapper
            if (!sourceWrapperItem)
                continue

            if (sourceWrapperItem.isMinimized)
                continue

            sourceSuefaceItem = sourceWrapperItem.surfaceItem
            if (!sourceSuefaceItem)
                continue

            sourceSuefaceItem.opacity = 1.0
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

            root.activeSurface = root.model.get(switchView.currentIndex).wrapper

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
                if (root.model.count > root.windowsExpandLimit) {
                    showAllInactiveSurfaces()
                } else {
                    inactiveWindowsExpand()
                }
                activeCurrentSurface(true)
            } else {
                root.visible = false
                activeCurrentSurface(false)
                showAllInactiveSurfaces()
            }
        } else {
            root.visible = false
        }
    }

    function inactiveWindowsExpand() {
        var hSpacing = 20
        var vSpacing = 20
        var sourceSuefaceItem
        var sourceWrapperItem
        var preferredHeight , preferredWidth , refHeight , height , width, offsetPoint
        for (var i = 0; i < switchView.count; ++i) {
            if (i === switchView.currentIndex)
                continue

            sourceWrapperItem = root.model.get(i).wrapper
            if (!sourceWrapperItem)
                continue

            if (sourceWrapperItem.isMinimized)
                continue

            sourceSuefaceItem = root.model.get(i).item
            if (!sourceSuefaceItem)
                continue

            preferredHeight = sourceSuefaceItem.height < (previewContentItem.height - 2 * vSpacing) ?
                        sourceSuefaceItem.height : (previewContentItem.height - 2 * vSpacing)
            preferredWidth = sourceSuefaceItem.width < (previewContentItem.width - 2 * hSpacing) ?
                        sourceSuefaceItem.width : (previewContentItem.width - 2 * hSpacing)
            refHeight = preferredHeight *  sourceSuefaceItem.width / sourceSuefaceItem.height < (previewContentItem.width - 2 * hSpacing)
            height = refHeight ? preferredHeight : preferredWidth * sourceSuefaceItem.height / sourceSuefaceItem.width
            width = refHeight ? preferredHeight * sourceSuefaceItem.width / sourceSuefaceItem.height : preferredWidth
            offsetPoint = sourceSuefaceItem.mapFromItem(currentContext.item, 0, 0)
            sourceWrapperItem.xFrom = sourceSuefaceItem.x
            sourceWrapperItem.yFrom = sourceSuefaceItem.y
            sourceWrapperItem.xTo = sourceWrapperItem.xFrom + offsetPoint.x + currentContext.item.width / 2.0 - width / 2.0
            sourceWrapperItem.yTo = sourceWrapperItem.yFrom + offsetPoint.y + currentContext.item.height / 2.0 - height / 2.0
            sourceWrapperItem.scaleTo = width / sourceSuefaceItem.width

            sourceSuefaceItem.x = sourceWrapperItem.xTo
            sourceSuefaceItem.y = sourceWrapperItem.yTo
            sourceSuefaceItem.opacity = 0
            sourceSuefaceItem.scale = sourceWrapperItem.scaleTo

            sourceWrapperItem.inHomeAni = true
            sourceWrapperItem.scaleStatus = 1
            sourceWrapperItem.scaleStatus = 0
        }
    }

    Item {
        id: previewContentItem

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: leftpreferredMargin
        Layout.rightMargin: rightpreferredMargin

        WindowPreviewLoader {
            id: preContext

            anchors.centerIn: parent
            sourceComponent: undefined
        }

        WindowPreviewLoader {
            id: currentContext

            anchors.centerIn: parent
            sourceComponent: undefined
            onClicked: {
                previewClicked()
                handleExit()
            }
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
                target: previewReductionAni.target
                from: 1.0
                to: previewReductionAni.scaleTo
                duration: previewReductionAni.duration
                easing.type: Easing.OutExpo
            }

            XAnimator {
                target: previewReductionAni.target
                from: previewReductionAni.xFrom
                to: previewReductionAni.xTo
                duration: previewReductionAni.duration
                easing.type: Easing.OutExpo
            }

            YAnimator {
                target: previewReductionAni.target
                from: previewReductionAni.yFrom
                to: previewReductionAni.yTo
                duration: previewReductionAni.duration
                easing.type: Easing.OutExpo
            }

            onFinished:  {
                root.visible = false
                previewReductionAni.target.scale = 1.0

                if (root.activeSurface) {
                    root.activeSurface.surfaceItem.opacity = 1
                }
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
                if (switchView.currentIndex < 0)
                    return

                if (root.stopWork)
                    return

                switchView.positionViewAtIndex(currentIndex, ListView.Center)

                if (!switchView.model)
                    return

                var preInidex

                if (switchView.count === 1) {
                    preInidex = -1
                } else if (switchView.currentIndex === 0) {
                    preInidex = switchView.count - 1
                } else {
                    preInidex = currentIndex - 1
                }

                if (preInidex > -1) {
                    const preSurfaceItem = switchView.model.get(preInidex).item
                    if (preSurfaceItem) {
                        preContext.sourceSueface = preSurfaceItem
                        preContext.cornerRadius = switchView.model.get(preInidex).wrapper.cornerRadius
                    }
                }

                const surfaceItem = switchView.model.get(currentIndex).item
                if (!surfaceItem)
                    return

                currentContext.sourceSueface = surfaceItem
                currentContext.cornerRadius = switchView.model.get(currentIndex).wrapper.cornerRadius
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

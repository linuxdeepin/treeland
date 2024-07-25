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
    readonly property int leftpreferredMargin: 20
    readonly property int rightpreferredMargin: 20
    readonly property real radius: enableRadius ? 18 : 0

    signal surfaceActivated(surface: SurfaceWrapper)

    spacing: 30

    onVisibleChanged: {
        if (visible) {
            root.showAllSurface = false
            previewReductionAni.stop()
            context.sourceComponent = undefined
            switchView.currentIndex = 0

            if (!switchView.model)
                return

            const surfaceItem = switchView.model.get(switchView.currentIndex).item
            if (!surfaceItem)
                return

            context.sourceSueface = surfaceItem
            ensurePreView()
        }
    }

    function ensurePreView() {
        if (context.sourceComponent && root.enableAnimation) {
            previewSwitchAni.stop()
            previewSwitchAni.isShowAni = false
            previewSwitchAni.start()
        }

        context.sourceComponent = previewComponent

        if (root.enableAnimation) {
            previewSwitchAni.stop()
            previewSwitchAni.isShowAni = true
            previewSwitchAni.start()
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

        if (context.sourceComponent) {
            if (root.enableAnimation) {
                switchViewContentAni.stop()
                switchViewContentAni.isInAni = false
                switchViewContentAni.start()
            }

            if (root.enableAnimation) {
                context.item.transformOrigin = Item.TopLeft
                previewReductionAni.target = context.item
                previewReductionAni.scaleTo = context.sourceSueface.width / context.width
                context.item.scale = 1.0
                context.item.x = 0
                context.item.y = 0
                previewReductionAni.xFrom = 0
                previewReductionAni.yFrom = 0
                var point = context.item.mapFromItem(root.model.get(switchView.currentIndex).item, 0, 0)
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

        Loader {
            id: context

            readonly property real hSpacing: 20
            readonly property real vSpacing: 20
            property SurfaceItem sourceSueface
            property real preferredHeight: sourceSueface.height < (parent.height - 2 * vSpacing) ?
                                               sourceSueface.height : (parent.height - 2 * vSpacing)
            property real preferredWidth: sourceSueface.width < (parent.width - 2 * hSpacing) ?
                                              sourceSueface.width : (parent.width - 2 * hSpacing)
            property bool refHeight: preferredHeight *  sourceSueface.width / sourceSueface.height < (parent.width - 2 * hSpacing)

            anchors.centerIn: parent
            height: refHeight ? preferredHeight : preferredWidth * sourceSueface.height / sourceSueface.width
            width: refHeight ? preferredHeight * sourceSueface.width / sourceSueface.height : preferredWidth
            sourceComponent: undefined
        }

        Component {
            id: previewComponent

            ShaderEffectSource {
                sourceItem: sourceSueface
                transformOrigin: Item.Center

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: function(mouse) {
                        handleExit()
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.NoButton
                    acceptedDevices: PointerDevice.TouchScreen
                    onDoubleTapped: function(eventPoint, button) {
                        handleExit()
                    }
                }
            }
        }

        ParallelAnimation {
            id: previewSwitchAni

            property bool isShowAni: true

            ScaleAnimator {
                target: context.item;
                from: previewSwitchAni.isShowAni ? 0.5 : 1.0;
                to: previewSwitchAni.isShowAni ? 1.0 : 0.5;
            }

            OpacityAnimator {
                target: context.item;
                from: previewSwitchAni.isShowAni ? 0.0 : 1.0;
                to: previewSwitchAni.isShowAni ? 1.0 : 0.0;
            }

            onFinished: context.item.scale = 1.0
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
            }

            XAnimator {
                target: previewReductionAni.target;
                from: previewReductionAni.xFrom
                to: previewReductionAni.xTo
                duration: previewReductionAni.duration
            }

            YAnimator {
                target: previewReductionAni.target;
                from: previewReductionAni.yFrom
                to: previewReductionAni.yTo
                duration: previewReductionAni.duration
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
                switchView.positionViewAtIndex(currentIndex, ListView.Center)

                if (!switchView.model)
                    return

                const surfaceItem = switchView.model.get(currentIndex).item
                if (!surfaceItem)
                    return

                context.sourceSueface = surfaceItem
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
            running: true
        }

        OpacityAnimator {
            target: switchViewContent
            from: switchViewContentAni.isInAni ? 0.0 : 1.0
            to: switchViewContentAni.isInAni ? 1.0 : 0.0
            duration: switchViewContentAni.duration
        }

        onStopped: switchViewContent.y = switchViewContentAni.fromeY
    }
}

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import Treeland
import Waylib.Server
import QtQuick.Effects
import org.deepin.dtk 1.0 as D

Item {
    id: root
    visible: false

    property bool switchOn: true
    property int focusReason: Qt.TabFocusReason
    required property QtObject output
    readonly property QtObject model: Helper.workspace.currentFilter

    // control all switch item
    property bool enableBlur: GraphicsInfo.api !== GraphicsInfo.Software
    property bool enableBorders: true
    property bool enableShadows: GraphicsInfo.api !== GraphicsInfo.Software
    property bool enableRadius: true
    property bool enableAnimation: GraphicsInfo.api !== GraphicsInfo.Software

    readonly property int leftpreferredMargin: 20
    readonly property int rightpreferredMargin: 20
    readonly property real radius: enableRadius ? 18 : 0

    width: output.validRect.width
    height: output.validRect.height

    onVisibleChanged: {
        if (visible) {
            mask.opacity = 0.5
            currentContext.loaderStatus = 0

            switchItemAnimation.stop()
            switchItemAnimation.isInAni = true
            switchItemAnimation.start()
        }
    }

    Connections {
        target: Helper.workspace.current

        onSurfaceAdded: function(surface) {
            if (root.visible && currentContext.visible) {
                surface.opacity = 0.0
            }
        }
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        color: "black"
        opacity: 0.0

        Behavior on opacity {
            enabled: root.enableAnimation

            NumberAnimation {
                duration: 400
                easing.type: Easing.OutExpo
            }
        }

        Component.onCompleted: {
            Qt.callLater(() => {
                mask.opacity = 0.5
            })
        }
    }

    MouseArea {
        anchors.fill: parent

        onClicked: {
            root.exit()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 20
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 54
        spacing: 20

        Item {
            id: previewItem

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 30

            TaskWindowPreview {
                id: previewContext
                visible: previewWindows.count === 0

                anchors.centerIn: previewItem
                sourceComponent: undefined
            }

            TaskWindowPreview {
                id: currentContext
                visible: previewWindows.count === 0

                sourceSurface: switchView.currentItem.surface
                anchors.centerIn: previewItem
                sourceComponent: undefined

                onClicked: {
                    root.exit()
                }
            }
        }

        Item {
            id: switchItem

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

            Blur {
                anchors.fill: parent
                visible: root.enableBlur
                radius: root.radius
                blurEnabled: root.enableBlur
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

                readonly property real titleheight: 38
                readonly property real fullheight: switchView.height
                readonly property real vMargin: 38
                readonly property real thumbnailheight: fullheight - titleheight
                readonly property real titleHMargin: 7
                readonly property real borderMargin: 4
                readonly property real vSpacing: 8
                readonly property size iconSize: Qt.size(24, 24)
                readonly property real borderWidth: borderMargin
                readonly property real delegateMinWidth: 128
                readonly property real delegateMaxWidth: 260
                readonly property real separatorHeight: 1
                readonly property real radius: root.enableRadius ? 20 : 0

                // control listview delegate and highlight
                property bool enableDelegateBlur: root.enableBlur
                property bool enableDelegateBorders: root.enableBorders
                property bool enableDelegateShadows: root.enableShadows
                property bool enableDelegateAnimation: root.enableAnimation
                property bool enableDelegateRadius: root.enableRadius


                orientation: ListView.Horizontal
                clip: true
                anchors {
                    fill: parent
                    leftMargin: vSpacing
                    rightMargin: vSpacing
                }

                model: root.model
                delegate: SwitchViewDelegate {
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            let pos = mapToItem(switchView.contentItem, mouse.x, mouse.y)
                            let index = switchView.indexAt(pos.x, pos.y)
                            switchIndex(index)
                        }
                    }
                }
                highlight: SwitchViewHighlightDelegate {}
                highlightFollowsCurrentItem: false

                Component.onCompleted: {
                    if (root.model.activeIndex >= 0 && root.model.activeIndex < switchView.count)
                        switchView.currentIndex = root.model.activeIndex;
                    else {
                        switchView.currentIndex = switchView.count > 1 ? 1 : 0
                    }
                }
            }
        }

        Component.onCompleted: {
            if (root.enableAnimation) {
                switchItemAnimation.fromeY = switchItem.y
                switchItemAnimation.stop()
                switchItemAnimation.isInAni = true
                switchItemAnimation.start()
            }
        }
    }

    ParallelAnimation {
        id: switchItemAnimation

        property bool isInAni: true
        property real fromeY
        readonly property int duration: 400
        readonly property real toY: root.height - 60

        YAnimator {
            target: switchItem
            from: switchItemAnimation.isInAni ? switchItemAnimation.toY : switchItemAnimation.fromeY
            to: switchItemAnimation.isInAni ? switchItemAnimation.fromeY : switchItemAnimation.toY
            duration: switchItemAnimation.duration
            easing.type: Easing.OutExpo
        }

        OpacityAnimator {
            target: switchItem
            from: switchItemAnimation.isInAni ? 0.0 : 1.0
            to: switchItemAnimation.isInAni ? 1.0 : 0.0
            duration: switchItemAnimation.duration
            easing.type: Easing.OutExpo
        }

        onStopped: switchItem.y = switchItemAnimation.fromeY
    }

    Repeater {
        id: previewWindows

        property bool reverse: false
        property int finishedAnimations: 0
        property int totalAnimations: previewWindows.count
        signal animationsFinished

        delegate: Item {
            id: windowItem
            required property SurfaceWrapper surface

            ParallelAnimation {
                id: previewAnimation

                property real scaleFrom
                property int  opacityFrom: 0.0
                property real xFrom
                property real yFrom
                property real xTo
                property real yTo
                property int duration: 400
                property Item target

                ScaleAnimator {
                    target: previewAnimation.target
                    from: previewWindows.reverse ? 1.0 : previewAnimation.scaleFrom
                    to: previewWindows.reverse ? previewAnimation.scaleFrom : 1.0
                    duration: previewAnimation.duration
                    easing.type: Easing.OutExpo
                }

                OpacityAnimator {
                    loops: Helper.activatedSurface === surface || surface.visible ? 1 : 0
                    target: previewAnimation.target;
                    from: previewWindows.reverse ? 1.0 : previewAnimation.opacityFrom
                    to: previewWindows.reverse ? previewAnimation.opacityFrom : 1.0
                    duration: previewAnimation.duration
                    easing.type: Easing.OutExpo
                }

                XAnimator {
                    target: previewAnimation.target
                    from: previewWindows.reverse ? previewAnimation.xTo : previewAnimation.xFrom
                    to: previewWindows.reverse ? previewAnimation.xFrom : previewAnimation.xTo
                    duration: previewAnimation.duration
                    easing.type: Easing.OutExpo
                }

                YAnimator {
                    target: previewAnimation.target
                    from: previewWindows.reverse ? previewAnimation.yTo : previewAnimation.yFrom
                    to: previewWindows.reverse ? previewAnimation.yFrom : previewAnimation.yTo
                    duration: previewAnimation.duration
                    easing.type: Easing.OutExpo
                }

                onFinished: {
                    previewWindows.finishedAnimations += 1

                    if (previewWindows.finishedAnimations === previewWindows.totalAnimations) {
                        previewWindows.animationsFinished()

                        previewWindows.finishedAnimations = 0
                        previewWindows.model = []
                    }
                }
            }

            Component.onCompleted: {
                if (!surface)
                    return

                var wh = previewPostion(surface, previewItem)
                previewAnimation.target = surface
                previewAnimation.xFrom = (40 + previewItem.width - wh.width) / 2.0
                previewAnimation.yFrom = (104 + previewItem.height - wh.height) / 2.0
                previewAnimation.xTo = surface.x
                previewAnimation.yTo = surface.y
                previewAnimation.scaleFrom = wh.width / surface.width

                if (surface === Helper.activatedSurface) {
                    surface.transformOrigin = Item.TopLeft
                    previewAnimation.opacityFrom = 1.0
                    surface.opacity = 1.0
                } else {
                    previewAnimation.duration = 500
                    surface.x = previewAnimation.xFrom
                    surface.opacity = 0.0
                    surface.y = previewAnimation.yFrom
                    surface.scale = previewAnimation.scaleFrom
                }

                previewAnimation.start()
            }
        }

        onAnimationsFinished: {
            if (!previewWindows.reverse) {
                showTask(false)
                root.switchOn = false
            }
        }
    }

    function previewPostion(surface, content) {
        const hSpacing = 20
        const vSpacing = 20
        let preferredHeight = surface.height < (content.height - 2 * vSpacing) ?
                                           surface.height : (content.height - 2 * vSpacing)
        let preferredWidth = surface.width < (content.width - 2 * hSpacing) ?
                                          surface.width : (content.width - 2 * hSpacing)
        let refHeight = preferredHeight *  surface.width / surface.height < (content.width - 2 * hSpacing)

        return {
            width: refHeight ? preferredHeight * surface.width / surface.height : preferredWidth,
            height: refHeight ? preferredHeight : preferredWidth * surface.height / surface.width
        }
    }

    function ensurePreview() {
        if (!previewContext.sourceComponent && root.enableAnimation)
            previewContext.sourceComponent = previewContext.previewComponent

        if (root.enableAnimation) {
            previewContext.loaderStatus = 1
            previewContext.loaderStatus = 0
        }

        if (!currentContext.sourceComponent)
            currentContext.sourceComponent = currentContext.previewComponent

        if (root.enableAnimation) {
            currentContext.loaderStatus = 0
            currentContext.loaderStatus = 1
        }
    }

    function previous() {
        if (switchView.count <= 1) {
            if (switchView.count === 1) {
                previewContext.sourceSurface = switchView.currentItem.surface
                if (!previewContext.sourceComponent && root.enableAnimation)
                    previewContext.sourceComponent = previewContext.previewComponent
                showTask(true);
            }
            return;
        }

        var nextIndex = (switchView.currentIndex - 1 + switchView.count) % switchView.count

        switchView.positionViewAtIndex(nextIndex, ListView.Beginning)
        if (switchView.contentWidth > root.width && nextIndex !== 0)
            switchView.contentX -= switchView.delegateMinWidth / 2

        focusReason = Qt.BacktabFocusReason
        switchIndex(nextIndex)
    }

    function next() {
        if (switchView.count <= 1) {
            if (switchView.count === 1) {
                previewContext.sourceSurface = switchView.currentItem.surface
                if (!previewContext.sourceComponent && root.enableAnimation)
                    previewContext.sourceComponent = previewContext.previewComponent
                showTask(true);
            }
            return;
        }

        var nextIndex = (switchView.currentIndex + 1) % switchView.count

        switchView.positionViewAtIndex(nextIndex, ListView.End)
        if (switchView.contentWidth > root.width && nextIndex !== switchView.count - 1)
            switchView.contentX += switchView.delegateMinWidth / 2

        focusReason = Qt.TabFocusReason
        switchIndex(nextIndex)
    }

    function switchIndex(next) {
        if ((next >= 0 && next < switchView.count) && showTask(true)) {
            previewContext.sourceSurface = switchView.currentItem.surface
            switchView.currentIndex = next
            root.model.activeIndex = switchView.currentIndex

            ensurePreview()
        }
    }

    function showTask(visible) {
        if (switchView.count === 0) {
            root.visible = false
            return false
        }

        Helper.workspace.current.opaque = !visible
        root.visible = visible

        return switchView.currentItem && switchView.visible
    }

    function exit() {
        if (!root.visible) {
            root.switchOn = false
            return
        }

        if (root.enableAnimation) {
            previewContext.loaderStatus = -1
            currentContext.loaderStatus = -1
        }

        if (root.enableAnimation) {
            mask.opacity = 0.0

            switchItemAnimation.stop()
            switchItemAnimation.isInAni = false
            switchItemAnimation.start()
        }

        if (switchView.currentItem)
            Helper.forceActivateSurface(switchView.currentItem.surface, focusReason)

        if (root.enableAnimation && switchView.count <= 18) {
            previewWindows.reverse = false
            previewWindows.model = root.model
        } else {
            previewWindows.reverse = false
            previewWindows.finishedAnimations = 0
            previewWindows.model = []
            showTask(false)
            root.switchOn = false
        }
    }
}

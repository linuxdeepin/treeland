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
    readonly property int leftpreferredMargin: 20
    readonly property int rightpreferredMargin: 20
    readonly property real radius: enableRadius ? 18 : 0

    signal surfaceActivated(surface: SurfaceWrapper)

    spacing: 30

    onVisibleChanged: {
        if (visible) {
            switchView.currentIndex = 0
        } else {
            stop()
        }
    }

    function previous() {
        switchView.currentIndex = switchView.currentIndex - 1
        if (switchView.currentIndex < 0) {
            switchView.currentIndex = switchView.count - 1;
        }
    }

    function next() {
        switchView.currentIndex = switchView.currentIndex + 1
        if (switchView.currentIndex >= switchView.count) {
            switchView.currentIndex = 0
        }
    }

    function stop() {
        context.sourceComponent = undefined

        const wrapper = model.get(switchView.currentIndex).wrapper
        // activated window changed
        if (wrapper) {
            surfaceActivated(wrapper)
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

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: function(mouse) {
                        root.visible = false
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.NoButton
                    acceptedDevices: PointerDevice.TouchScreen
                    onDoubleTapped: function(eventPoint, button) {
                        root.visible = false
                    }
                }
            }
        }
    }

    Item {
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
                context.sourceComponent = previewComponent
            }
        }
    }
}

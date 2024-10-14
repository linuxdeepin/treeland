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

    required property QtObject output
    readonly property OutputItem outputItem: output.outputItem

    // control all switch item
    property bool enableBlur: GraphicsInfo.api !== GraphicsInfo.Software
    property bool enableBorders: true
    property bool enableShadows: GraphicsInfo.api !== GraphicsInfo.Software
    property bool enableRadius: true
    property bool enableAnimation: GraphicsInfo.api !== GraphicsInfo.Software

    readonly property int leftpreferredMargin: 20
    readonly property int rightpreferredMargin: 20

    width: outputItem.width
    height: outputItem.height

    WorkSpaceMask {
        id: mask

        anchors.fill: parent
    }

    MouseArea {
        anchors.fill: parent
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 20

        Item {
            id: previewItem

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: leftpreferredMargin
            Layout.rightMargin: rightpreferredMargin

            SurfaceProxy {
                id: activeSurface
                live: true
                anchors.centerIn: previewItem
                surface: Helper.activatedSurface
                maxSize: Qt.size(640, 480)
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
                anchors {
                    fill: parent
                    leftMargin: vSpacing
                    rightMargin: vSpacing
                }

                model: Helper.workspace.current.model
                delegate: SwitchViewDelegate {}
                highlight: SwitchViewHighlightDelegate {}
                highlightFollowsCurrentItem: false
            }
        }
    }

    function previous() {
        var previousIndex = (switchView.currentIndex - 1 + switchView.count) % switchView.count
        switchView.currentIndex = previousIndex
        Helper.activateSurface(switchView.currentItem.surface, Qt.BacktabFocusReason)
    }

    function next() {
        var nextIndex = (switchView.currentIndex + 1) % switchView.count
        switchView.currentIndex = nextIndex
        Helper.activateSurface(switchView.currentItem.surface, Qt.TabFocusReason)
    }
}

// Copyright (C) 2024 ShanShan Ye <847862258@qq.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland
import org.deepin.dtk as D
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root
    visible: enabled
    focusPolicy: Qt.TabFocus
    D.ColorSelector.inactived: false

    property D.Palette backgroundColor: D.Palette {
        normal: Qt.rgba(1.0, 1.0, 1.0, 0.3)
        hovered: Qt.rgba(1.0, 1.0, 1.0, 0.3)
        pressed: Qt.rgba(1.0, 1.0, 1.0, 0.5)
        normalDark: Qt.rgba(1.0, 1.0, 1.0, 0.3)
        hoveredDark: Qt.rgba(1.0, 1.0, 1.0, 0.3)
        pressedDark: Qt.rgba(1.0, 1.0, 1.0, 0.5)
    }

    property D.Palette focusBackgroundColor: D.Palette {
        normal: Qt.rgba(1.0, 1.0, 1.0, 0.1)
        hovered: Qt.rgba(1.0, 1.0, 1.0, 0.3)
        pressed: Qt.rgba(1.0, 1.0, 1.0, 0.5)
        normalDark: Qt.rgba(1.0, 1.0, 1.0, 0.1)
        hoveredDark: Qt.rgba(1.0, 1.0, 1.0, 0.3)
        pressedDark: Qt.rgba(1.0, 1.0, 1.0, 0.5)
    }

    property D.Palette contentTextColor: D.Palette {
        normal: Qt.rgba(1.0, 1.0, 1.0, 1.0)
        hovered: Qt.rgba(0.0, 0.0, 0.0, 0.7)
        pressed: Qt.rgba(0.0, 0.0, 0.0, 0.7)
        normalDark: Qt.rgba(1.0, 1.0, 1.0, 1.0)
        hoveredDark: Qt.rgba(0.0, 0.0, 0.0, 0.7)
        pressedDark: Qt.rgba(0.0, 0.0, 0.0, 0.7)
    }

    palette.windowText: root.D.ColorSelector.contentTextColor
    palette.buttonText: root.D.ColorSelector.contentTextColor

    icon {
        width: 40
        height: 40
    }

    contentItem: Item {
        implicitWidth: btn.width
        implicitHeight: btn.height + txt.height + txt.anchors.topMargin
        Control {
            id: btn
            width: 84
            height: 84
            contentItem: D.DciIcon {
                palette: D.DTK.makeIconPalette(root.palette)
                mode: root.D.ColorSelector.controlState
                theme: root.D.ColorSelector.controlTheme
                name: root.icon.name
                sourceSize: Qt.size(icon.width, icon.height)
            }

            background: Item {
                RoundBlur {
                    anchors.fill: parent
                    radius: btn.width / 2
                    color: root.activeFocus
                           ? root.D.ColorSelector.focusBackgroundColor
                           : root.D.ColorSelector.backgroundColor
                }
                D.FocusBoxBorder {
                    visible: root.activeFocus
                    anchors.fill: parent
                    anchors.margins: 1
                    borderWidth: 3
                    radius: width / 2
                    color: Qt.rgba(1.0, 1.0, 1.0, 0.3)
                }
            }
        }

        Label {
            id: txt
            anchors {
                top: btn.bottom
                topMargin: 20
                horizontalCenter: btn.horizontalCenter
            }
            leftPadding: 10
            rightPadding: 10
            topPadding: 4
            bottomPadding: 4
            text: root.text
            color: root.D.ColorSelector.contentTextColor

            background: Item {
                visible: root.pressed || root.hovered || root.activeFocus
                RoundBlur {
                    anchors.fill: parent
                    radius: 6
                    color: root.activeFocus 
                           ? root.D.ColorSelector.focusBackgroundColor
                           : root.D.ColorSelector.backgroundColor
                }
                D.FocusBoxBorder {
                    visible: root.activeFocus
                    anchors.fill: parent
                    anchors.margins: 1
                    borderWidth: 2
                    radius: 6
                    color: Qt.rgba(1.0, 1.0, 1.0, 0.3)
                }
            }
        }
    }
    background: null
}

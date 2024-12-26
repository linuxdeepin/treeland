// Copyright (C) 2024 ShanShan Ye <847862258@qq.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland
import org.deepin.dtk as D
import QtQuick.Controls
import QtQuick.Layouts

D.Button {
    id: root
    visible: enabled

    property D.Palette backgroundColor: D.Palette {
        normal: Qt.rgba(1.0, 1.0, 1.0, 0.3)
        hovered: Qt.rgba(1.0, 1.0, 1.0, 0.5)
        pressed: Qt.rgba(1.0, 1.0, 1.0, 0.1)
    }
    textColor: D.Palette {
        normal: Qt.rgba(1.0, 1.0, 1.0, 1.0)
        hovered: Qt.rgba(0.0, 0.0, 0.0, 0.7)
        pressed: Qt.rgba(0.0, 0.0, 0.0, 0.7)
    }

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
                    color: root.D.ColorSelector.backgroundColor
                }
                D.OutsideBoxBorder {
                    anchors.fill: parent
                    visible: root.pressed
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
            color: root.D.ColorSelector.textColor

            background: Item {
                visible: root.pressed || root.hovered
                RoundBlur {
                    anchors.fill: parent
                    radius: 6
                    color: root.D.ColorSelector.backgroundColor
                }
                D.OutsideBoxBorder {
                    visible: root.pressed
                    anchors.fill: parent
                    borderWidth: 2
                    radius: 6
                    color: Qt.rgba(1.0, 1.0, 1.0, 0.3)
                }
            }
        }
    }
    background: null
}

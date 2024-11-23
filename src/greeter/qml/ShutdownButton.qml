// Copyright (C) 2024 ShanShan Ye <847862258@qq.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland.Greeter
import org.deepin.dtk as D
import QtQuick.Controls
import QtQuick.Layouts

D.Button {
    id: root

    property D.Palette backgroundColor: D.Palette {
        normal: Qt.rgba(1.0, 1.0, 1.0, 0.10)
        hovered: Qt.rgba(1.0, 1.0, 1.0, 127 / 255.0)
        pressed: Qt.rgba(0, 15 / 255.0, 39 / 255.0, 178 / 255.0)
    }

    icon {
        width: 50
        height: 50
    }

    contentItem: Item {
        implicitWidth: btn.width
        implicitHeight: btn.height + txt.height + txt.anchors.topMargin
        Control {
            id: btn
            width: 60
            height: 60
            contentItem: D.DciIcon {
                palette: D.DTK.makeIconPalette(root.palette)
                mode: root.D.ColorSelector.controlState
                theme: root.D.ColorSelector.controlTheme
                name: root.icon.name
                sourceSize: Qt.size(icon.width, icon.height)
            }
            background: RoundBlur {
                radius: btn.width / 2
                color: root.D.ColorSelector.backgroundColor
            }
        }

        Label {
            id: txt
            anchors {
                top: btn.bottom
                topMargin: 15
                horizontalCenter: btn.horizontalCenter
            }
            padding: 2
            text: root.text
            background: RoundBlur {
                radius: 4
                visible: root.hovered && root.enabled
                color: root.D.ColorSelector.backgroundColor
            }
        }
    }
    background: null
}

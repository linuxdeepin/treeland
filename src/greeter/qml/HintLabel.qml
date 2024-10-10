// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import Treeland.Greeter
import org.deepin.dtk 1.0 as D
import QtQuick.Controls

D.Popup {
    property string hintText
    modal: true
    background: D.FloatingPanel {
        anchors.fill: parent
        radius: 12
        blurRadius: 12
        backgroundColor: D.Palette {
            normal: Qt.rgba(238 / 255, 238 / 255, 238 / 255, 0.3)
        }
    }

    ColumnLayout {
        Layout.alignment: Qt.AlignLeft
        Text {
            text: qsTr("Password Hint")
            color: Qt.rgba(1, 1, 1, 0.7)
            font: D.fontManager.t10
        }
        Text {
            text: hintText
            color: "white"
            font: D.fontManager.t10
        }
    }
}

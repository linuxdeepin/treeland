// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import org.deepin.dtk 1.0 as D
import Treeland

RowLayout {
    id: bottomGroup
    property var buttonSize: 30
    spacing: 15

    Item {
        implicitWidth: bottomGroup.buttonSize + 6
        implicitHeight: bottomGroup.buttonSize + 6
        Layout.alignment: Qt.AlignHCenter
        visible: userList.count > 1

        D.RoundButton {
            id: usersBtn

            property bool expand: false
            icon.name: "login_user"
            icon.width: 16
            icon.height: 16
            width: expand ? bottomGroup.buttonSize + 6 : bottomGroup.buttonSize
            height: expand ? bottomGroup.buttonSize + 6 : bottomGroup.buttonSize
            anchors.centerIn: parent
            hoverEnabled: parent.visible

            D.ToolTip.visible: hovered
            D.ToolTip.text: qsTr("Other Users")

            UserList {
                id: userList
                x: (usersBtn.width - userList.width) / 2 - 10
                y: -userList.height - 10
                onClosed: usersBtn.expand = false
            }

            onClicked: {
                usersBtn.expand = true
                userList.open()
            }

            background: RoundBlur {
                radius: usersBtn.width / 2
            }
        }
    }

    Item {
        implicitWidth: bottomGroup.buttonSize + 6
        implicitHeight: bottomGroup.buttonSize + 6
        Layout.alignment: Qt.AlignHCenter
        D.RoundButton {
            id: powerBtn

            property bool expand: false
            icon.name: "login_power"
            icon.width: 16
            icon.height: 16
            width: expand ? bottomGroup.buttonSize + 6 : bottomGroup.buttonSize
            height: expand ? bottomGroup.buttonSize + 6 : bottomGroup.buttonSize
            anchors.centerIn: parent
            D.ToolTip.visible: hovered
            D.ToolTip.text: qsTr("Power")
            PowerList {
                id: powerList
                y: -powerList.height - 10
                x: (powerBtn.width - powerList.width) / 2 - 10
                onClosed: powerBtn.expand = false
            }
            onClicked: {
                powerBtn.expand = true
                powerList.open()
            }

            background: RoundBlur {
                radius: powerBtn.width / 2
            }
        }
    }
}

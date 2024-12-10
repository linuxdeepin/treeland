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
    property int buttonSize: 30
    spacing: 15

    property bool powerVisible: powerList.visible
    function showUserList()
    {
        usersBtn.expand = true
        userList.open()
    }

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

            D.ToolTip {
                enabled: false
                visible: hovered
                text: qsTr("Other Users")
            }

            UserList {
                id: userList
                x: (usersBtn.width - userList.width) / 2 - 10
                y: -userList.height - 10
                onClosed: usersBtn.expand = false
            }

            onClicked: {
                showUserList()
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

            D.ToolTip {
                enabled: true
                visible: powerBtn.hovered
                text: qsTr("Power")
            }

            Popup {
                id: powerList
                width: powerBtn.Window.width
                height: 140
                parent: powerBtn.Window.contentItem
                x: 0
                y: powerBtn.Window.height / 5 * 2
                contentItem: PowerList { }
                background: MouseArea {
                    onClicked: function () {
                        powerBtn.expand = false
                        powerList.close()
                    }
                }
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

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
    required property Item rootItem
    signal lock()
    function showUserList()
    {
        userItem.expand = true
        userList.open()
    }

    ControlActionItem {
        id: userItem
        Layout.alignment: Qt.AlignHCenter
        visible: userList.count > 1
        iconName: "login_user"

        UserList {
            id: userList
            x: (userItem.width - userList.width) / 2 - 10
            y: -userList.height - 10
            onClosed: userItem.expand = false
        }

        onClicked: {
            showUserList()
        }
    }

    ControlActionItem {
        id: powerItem
        Layout.alignment: Qt.AlignHCenter
        iconName: "login_power"

        function closePopup() {
            powerList.close()
        }

        D.ToolTip {
            enabled: true
            visible: powerItem.hovered
            text: qsTr("Power")
        }

        Popup {
            id: powerList
            width: rootItem.width
            height: 140
            parent: rootItem
            x: 0
            y: rootItem.height / 5 * 2
            modal: true
            contentItem: PowerList { }
            background: MouseArea {
                onClicked: powerItem.closePopup()
            }
            onClosed: powerItem.expand = false
        }
        onClicked: {
            powerItem.expand = true
            powerList.open()
        }
    }

    component ControlActionItem: Item {
        id: actionItem
        property bool expand: false
        property string iconName
        signal clicked()
        implicitWidth: bottomGroup.buttonSize + 6
        implicitHeight: bottomGroup.buttonSize + 6
        D.RoundButton {
            icon {
                width: 16
                height: 16
                name: actionItem.iconName
            }

            Behavior on width {
                NumberAnimation {
                    duration: 150
                }
            }
            Behavior on height {
                NumberAnimation {
                    duration: 150
                }
            }
            width: actionItem.expand ? bottomGroup.buttonSize + 6 : bottomGroup.buttonSize
            height: actionItem.expand ? bottomGroup.buttonSize + 6 : bottomGroup.buttonSize
            anchors.centerIn: parent

            background: RoundBlur {
                radius: parent.width / 2
                color: Qt.rgba(1.0, 1.0, 1.0, 0.3)
            }
            onClicked: actionItem.clicked()
        }
    }
}

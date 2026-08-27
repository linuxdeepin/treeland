// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import org.deepin.dtk 1.0 as D
import Treeland

RowLayout {
    id: bottomGroup
    spacing: 15

    required property Item rootItem
    property int buttonSize: 30
    property bool powerVisible: powerList.visible

    signal otherUserRequested()

    /***************/
    /* Components */
    /**************/

    // TODO: Design the interface of session selection
    Button {
        id: sessionItem
        Layout.alignment: Qt.AlignHCenter
        visible: !GreeterProxy.hasActiveSession

        contentItem: D.IconLabel {
            text: SessionModel.data(SessionModel.index(SessionModel.currentIndex, 0), SessionModel.NameRole)
            color: "white"
        }

        SessionList {
            id: sessionList
            x: (sessionItem.width - sessionList.width) / 2 - 10
            y: -sessionList.height - 10
        }

        background: RoundBlur {
            objectName: "sessionItemBlur"
            radius: parent.width / 2
            color: Qt.rgba(1.0, 1.0, 1.0, 0.3)
        }

        onClicked: {
            sessionList.open()
        }
    }

    ControlActionItem {
        id: userItem
        Layout.alignment: Qt.AlignHCenter
        visible: userList.count > 1 || Helper.globalConfig.showOtherUserOption
        iconName: "login_user"

        UserList {
            id: userList
            x: (userItem.width - userList.width) / 2 - 10
            y: -userList.height - 10
            onClosed: userItem.expand = false
            onOtherUserRequested: bottomGroup.otherUserRequested()
        }

        onClicked: {
            showUserList()
        }
    }

    ControlActionItem {
        id: powerItem
        Layout.alignment: Qt.AlignHCenter
        iconName: "login_power"

        ToolTip {
            enabled: true
            visible: powerItem.hovered
            text: qsTr("Power")
        }

        Item {
            id: powerList
            parent: rootItem
            visible: powerItem.expand
            width: rootItem.width
            height: rootItem.height
            x: 0
            y: 0

            // Click outside the PowerList to close
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    powerItem.expand = false
                    innerPowerList.loopInside = false
                }
            }

            PowerList {
                id: innerPowerList
                width: rootItem.width
                height: 140
                x: 0
                y: rootItem.height / 5 * 2
            }
        }
        onClicked: {
            powerItem.expand = true
            innerPowerList.focusPowerOff()
            innerPowerList.enableLoopInside()
        }
    }

    component ControlActionItem: Item {
        id: actionItem
        property bool expand: false
        property string iconName
        property alias hovered: button.hovered
        signal clicked()
        implicitWidth: bottomGroup.buttonSize + 6
        implicitHeight: bottomGroup.buttonSize + 6
        RoundButton {
            id: button
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
                objectName: actionItem.iconName + "Blur"
                radius: parent.width / 2
                color: Qt.rgba(1.0, 1.0, 1.0, 0.3)
            }
            onClicked: actionItem.clicked()
        }
    }

    /*****************************/
    /* Functions and Connections */
    /*****************************/

    function showUserList() {
        userItem.expand = true
        userList.open()
    }
}

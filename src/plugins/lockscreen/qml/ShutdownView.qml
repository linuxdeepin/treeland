// Copyright (C) 2024 ShanShan Ye <847862258@qq.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland.Greeter
import QtQuick.Controls

FocusScope {
    id: root

    signal clicked()
    signal switchUser()
    signal lock()

    MouseArea {
        anchors.fill: parent
        enabled: true
        onClicked: root.clicked()
    }

    PowerList {

        width: parent.width
        height: 100
        anchors {
            top: parent.top
            topMargin: parent.height / 5 * 2
            horizontalCenter: parent.horizontalCenter
        }

        leftModelChildren: ShutdownButton {
            text: qsTr("lock")
            icon.name: "login_lock"
            onClicked: root.lock()
        }

        modelChildren: ShutdownButton {
            text: qsTr("switch user")
            icon.name: "login_user"
            onClicked: root.switchUser()
        }
    }
}

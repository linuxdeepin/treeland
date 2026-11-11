// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland
import QtQuick.Controls

FocusScope {
    id: root

    signal switchUser()

    // The menu is opened from the dock, where nothing holds the keyboard focus
    // yet: the lock button is the entry point when it is shown at all. Focus
    // does not come back on its own when an ancestor only becomes visible later,
    // so re-apply it then.
    Component.onCompleted: powerList.focusItem(lockBtn)
    onVisibleChanged: {
        if (visible)
            powerList.focusItem(lockBtn)
    }

    MouseArea {
        anchors.fill: parent
        enabled: true
        onClicked: GreeterProxy.showShutdownView = false
    }

    PowerList {
        id: powerList
        width: parent.width
        height: 140
        anchors {
            top: parent.top
            topMargin: parent.height / 5 * 2
            horizontalCenter: parent.horizontalCenter
        }

        modelChildren: [
            ShutdownButton {
                id: lockBtn
                visible: !GreeterProxy.isLocked
                Keys.forwardTo: [powerList]
                text: qsTr("lock")
                icon.name: "login_lock"
                onClicked: GreeterProxy.lock()
            },
            ShutdownButton {
                id: switchBtn
                visible: !GreeterProxy.isLocked
                Keys.forwardTo: [powerList]
                text: qsTr("switch user")
                icon.name: "login_switchuser"
                enabled: UserModel.count > 1
                onClicked: root.switchUser()
            },
            ShutdownButton {
                id: logoutBtn
                visible: !GreeterProxy.isLocked
                Keys.forwardTo: [powerList]
                text: qsTr("Logout")
                icon.name: "login_logout"
                onClicked: GreeterProxy.logout()
            }
        ]
    }
}

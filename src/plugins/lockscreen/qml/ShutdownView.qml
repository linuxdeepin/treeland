// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: root

    signal switchUser()
    signal outsideClicked()

    Component.onCompleted: {
        if (root.visible) {
            focusFirstButton()
        }
    }

    function focusFirstButton() {
        if (lockBtn.visible) {
            lockBtn.forceActiveFocus()
        } else {
            powerOffBtn.forceActiveFocus()
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.outsideClicked()
    }

    RowLayout {
        id: layout
        spacing: 100
        anchors {
            top: parent.top
            topMargin: parent.height / 5 * 2
            horizontalCenter: parent.horizontalCenter
        }

        ShutdownButton {
            id: lockBtn
            visible: !GreeterProxy.isLocked
            text: qsTr("lock")
            icon.name: "login_lock"
            onClicked: GreeterProxy.lock()
            KeyNavigation.tab: switchBtn
        }

        ShutdownButton {
            id: switchBtn
            visible: !GreeterProxy.isLocked
            text: qsTr("switch user")
            icon.name: "login_switchuser"
            enabled: UserModel.count > 1
            onClicked: root.switchUser()
            KeyNavigation.tab: logoutBtn
            KeyNavigation.backtab: lockBtn
        }

        ShutdownButton {
            id: logoutBtn
            visible: !GreeterProxy.isLocked
            text: qsTr("Logout")
            icon.name: "login_logout"
            onClicked: GreeterProxy.logout()
            KeyNavigation.tab: powerOffBtn
            KeyNavigation.backtab: switchBtn
        }

        ShutdownButton {
            id: powerOffBtn
            enabled: GreeterProxy.canPowerOff
            text: qsTr("Shut Down")
            icon.name: "login_shutdown"
            onClicked: GreeterProxy.powerOff()
            KeyNavigation.tab: rebootBtn
            // 锁屏态下 logoutBtn 不可见，backtab 不跳转避免焦点落到不可见按钮
            KeyNavigation.backtab: GreeterProxy.isLocked ? null : logoutBtn
        }

        ShutdownButton {
            id: rebootBtn
            enabled: GreeterProxy.canReboot
            text: qsTr("Reboot")
            icon.name: "login_reboot"
            onClicked: GreeterProxy.reboot()
            KeyNavigation.tab: suspendBtn
            KeyNavigation.backtab: powerOffBtn
        }

        ShutdownButton {
            id: suspendBtn
            enabled: GreeterProxy.canSuspend
            text: qsTr("Suspend")
            icon.name: "login_suspend"
            onClicked: GreeterProxy.suspend()
            KeyNavigation.tab: hibernateBtn
            KeyNavigation.backtab: rebootBtn
        }

        ShutdownButton {
            id: hibernateBtn
            enabled: GreeterProxy.canHibernate
            text: qsTr("Hibernate")
            icon.name: "login_hibernate"
            onClicked: GreeterProxy.hibernate()
            KeyNavigation.backtab: suspendBtn
        }
    }
}

// Copyright (C) 2023 ComixHe <heyuming@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland.Greeter
import org.deepin.dtk 1.0 as D
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: root
    property alias modelChildren: objModel.children

    implicitWidth: layout.width
    implicitHeight: layout.height

    RowLayout {
        id: layout
        spacing: 100
        anchors.centerIn: parent
        Repeater {
            model: ObjectModel {
                id: objModel

                ShutdownButton {
                    enabled: GreeterModel.proxy.canHibernate
                    text: qsTr("Hibernate")
                    icon.name: "login_sleep"
                    onClicked: GreeterModel.proxy.hibernate()
                }

                ShutdownButton {
                    enabled: GreeterModel.proxy.canSuspend
                    text: qsTr("Suspend")
                    icon.name: "login_suspend"
                    onClicked: GreeterModel.proxy.suspend()
                }

                ShutdownButton {
                    enabled: GreeterModel.proxy.canReboot
                    text: qsTr("Reboot")
                    icon.name: "login_reboot"
                    onClicked: GreeterModel.proxy.reboot()
                }

                ShutdownButton {
                    id: powerOff
                    enabled: GreeterModel.proxy.canPowerOff
                    text: qsTr("Shut Down")
                    icon.name: "login_poweroff"
                    onClicked: GreeterModel.proxy.powerOff()
                }
            }
        }
    }
}

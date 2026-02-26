// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland
import LockScreen
import org.deepin.dtk 1.0 as D
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: root
    property alias modelChildren: objModel.children
    property alias leftModelChildren: leftObjeModel.children

    implicitWidth: layout.width
    implicitHeight: layout.height

    RowLayout {
        id: layout
        spacing: 100
        anchors.centerIn: parent
        Repeater {
            model: ObjectModel {
            id: leftObjeModel
            }
        }
        Repeater {
            model: ObjectModel {
                id: objModel

                ShutdownButton {
                    id: powerOff
                    enabled: GreeterProxy.canPowerOff
                    text: qsTr("Shut Down")
                    icon.name: "login_shutdown"
                    onClicked: GreeterProxy.powerOff()
                }

                ShutdownButton {
                    enabled: GreeterProxy.canReboot
                    text: qsTr("Reboot")
                    icon.name: "login_reboot"
                    onClicked: GreeterProxy.reboot()
                }

                ShutdownButton {
                    enabled: GreeterProxy.canSuspend
                    text: qsTr("Suspend")
                    icon.name: "login_suspend"
                    onClicked: {
                        GreeterProxy.suspend()
                    }
                }

                ShutdownButton {
                    enabled: GreeterProxy.canHibernate
                    text: qsTr("Hibernate")
                    icon.name: "login_hibernate"
                    onClicked: {
                        GreeterProxy.hibernate()
                    }
                }
            }
        }
    }
}

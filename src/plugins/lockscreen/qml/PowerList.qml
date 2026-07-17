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

    property alias listPowerOffBtn: powerOffBtn
    property alias listHibernateBtn: hibernateBtn

    property bool loopInside: false

    signal tabOutForward()                                                                                                                                                 
    signal tabOutBackward()  

    implicitWidth: layout.width
    implicitHeight: layout.height

    function focusPowerOff() {
        powerOffBtn.forceActiveFocus()
    }
    function focusHibernate() {
        hibernateBtn.forceActiveFocus()
    }
    function enableLoopInside() {
        loopInside = true
    }
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
                    id: powerOffBtn
                    enabled: GreeterProxy.canPowerOff
                    text: qsTr("Shut Down")
                    icon.name: "login_shutdown"
                    onClicked: GreeterProxy.powerOff()
                    KeyNavigation.tab: rebootBtn
                    KeyNavigation.backtab: hibernateBtn
                    Keys.onBacktabPressed :function(event) {
                        if(root.loopInside)
                        {
                            event.accepted = false
                        }
                        else
                        {
                            root.tabOutBackward() 
                            event.accepted = true
                        }
                    }
                }

                ShutdownButton {
                    id:rebootBtn
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
                    onClicked: {
                        GreeterProxy.suspend()
                    }
                    KeyNavigation.tab: hibernateBtn
                    KeyNavigation.backtab: rebootBtn
                }

                ShutdownButton {
                    id: hibernateBtn
                    enabled: GreeterProxy.canHibernate
                    text: qsTr("Hibernate")
                    icon.name: "login_hibernate"
                    onClicked: {
                        GreeterProxy.hibernate()
                    }
                    KeyNavigation.tab: powerOffBtn
                    KeyNavigation.backtab: suspendBtn
                    Keys.onTabPressed :function(event) {
                        if(root.loopInside)
                        {
                            event.accepted = false
                        }
                        else
                        {
                            root.tabOutForward() 
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }
}

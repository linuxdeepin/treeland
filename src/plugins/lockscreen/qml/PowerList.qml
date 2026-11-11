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

    implicitWidth: layout.width
    implicitHeight: layout.height

    // A hidden or disabled button cannot take the focus (visible already covers
    // hidden ancestors), so fall back to the first button that can: the arrow
    // keys need a starting point. When nothing can, nothing is focused.
    function focusItem(item) {
        if (item.visible && item.enabled) {
            item.forceActiveFocus()
        } else {
            const items = navigableItems()
            if (items.length > 0)
                items[0].forceActiveFocus()
        }
    }

    function focusPowerOff() {
        focusItem(powerOffBtn)
    }

    // The focus chain is the row's buttons in visual order: the power buttons
    // declared here, plus the session buttons (lock / switch user / logout) the
    // view injects through modelChildren. It is the ObjectModel's own children
    // array, hence already in the order the Repeater lays out. A hidden or
    // disabled button steps aside (a disabled button hides itself).
    function navigableItems() {
        return modelChildren.filter(item => item.visible && item.enabled)
    }

    function moveFocus(step) {
        const items = navigableItems()
        if (items.length === 0)
            return
        let index = -1
        for (let i = 0; i < items.length; ++i) {
            if (items[i].activeFocus) {
                index = i
                break
            }
        }
        const next = index < 0 ? (step > 0 ? 0 : items.length - 1)
                               : (index + step + items.length) % items.length
        items[next].forceActiveFocus()
    }

    // The buttons handle neither the arrow keys nor Tab themselves, so the
    // scope moves the focus relative to the current item, wrapping at both ends.
    // Tab is a focus-traversal key: it only reaches the focused button and the
    // window handles it before any ancestor, so the buttons forward it here
    // (Keys.forwardTo) — otherwise Qt would tab out of the row.
    Keys.onLeftPressed: function(event) {
        moveFocus(-1)
        event.accepted = true
    }
    Keys.onRightPressed: function(event) {
        moveFocus(1)
        event.accepted = true
    }
    Keys.onTabPressed: function(event) {
        moveFocus(1)
        event.accepted = true
    }
    Keys.onBacktabPressed: function(event) {
        moveFocus(-1)
        event.accepted = true
    }

    RowLayout {
        id: layout
        spacing: 100
        anchors.centerIn: parent
        Repeater {
            model: ObjectModel {
                id: objModel

                ShutdownButton {
                    id: powerOffBtn
                    enabled: GreeterProxy.canPowerOff
                    Keys.forwardTo: [root]
                    text: qsTr("Shut Down")
                    icon.name: "login_shutdown"
                    onClicked: GreeterProxy.powerOff()
                }

                ShutdownButton {
                    id: rebootBtn
                    enabled: GreeterProxy.canReboot
                    Keys.forwardTo: [root]
                    text: qsTr("Reboot")
                    icon.name: "login_reboot"
                    onClicked: GreeterProxy.reboot()
                }

                ShutdownButton {
                    id: suspendBtn
                    enabled: GreeterProxy.canSuspend
                    Keys.forwardTo: [root]
                    text: qsTr("Suspend")
                    icon.name: "login_suspend"
                    onClicked: {
                        GreeterProxy.suspend()
                    }
                }

                ShutdownButton {
                    id: hibernateBtn
                    enabled: GreeterProxy.canHibernate
                    Keys.forwardTo: [root]
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

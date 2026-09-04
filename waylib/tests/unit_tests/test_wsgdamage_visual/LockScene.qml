// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server

// Replica of treeland lockscreen UserInput + ControlAction session chip.
// Qt Quick Controls + waylib RoundBlur (RenderBufferBlitter). No compositor
// plugin QML: those types are not available in this unit test. No DTK:
// waylib-only CI images do not ship org.deepin.dtk.
Item {
    id: root
    anchors.fill: parent

    readonly property alias passwordField: passwordField
    readonly property alias passwordBlur: passwordBlur
    readonly property Item caret: lockCaretItem
    readonly property alias sessionButton: sessionButton
    readonly property alias sessionPopup: sessionPopup
    readonly property alias sessionList: sessionList
    readonly property alias sessionGlass: sessionGlass

    property Item lockCaretItem

    // LoginAnimation replica: ShaderEffectSource { hideSource: true }
    // renders lock content into a Qt RT that is not a wlr buffer.
    property bool loginHideSource: false

    // Replica of DTK UserList/SessionList. Keep the panel in the output
    // tree (not Qt Popup): Popup.Item + opacity/scale snapshots the subtree
    // to a QSGLayer, so the 220x280 blitter never extra-QRhi copies.
    property bool popupEnterAnimation: false

    Item {
        id: lockContent
        objectName: "lockContent"
        anchors.fill: parent

        Rectangle {
            id: behindPulse
            objectName: "lockBehindPulse"
            x: 48
            y: 96
            width: 160
            height: 160
            z: 0
            color: Qt.rgba(0.2, 0.6, 1.0, 0.45)
        }

        // UserInput: 220x300, vertically centered in the right half.
        Item {
            id: userInput
            objectName: "lockUserInput"
            width: 220
            height: 300
            x: Math.round(root.width * 0.42)
            y: Math.round((root.height - height) / 2)

            Column {
                id: userCol
                spacing: 15
                anchors.centerIn: parent
                width: parent.width

                Rectangle {
                    objectName: "lockAvatar"
                    width: 120
                    height: 120
                    anchors.horizontalCenter: parent.horizontalCenter
                    radius: 20
                    color: Qt.rgba(1, 1, 1, 0.12)
                    border.width: 2
                    border.color: Qt.rgba(1, 1, 1, 0.1)
                }

                Text {
                    objectName: "lockUsername"
                    text: "User"
                    font.bold: true
                    font.pixelSize: 20
                    color: "white"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                TextField {
                    id: passwordField
                    objectName: "lockPassword"
                    width: userInput.width
                    implicitWidth: userInput.width
                    height: 30
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: TextInput.AlignHCenter
                    echoMode: showPasswordBtn.hiddenPWD ? TextInput.Password : TextInput.Normal
                    rightPadding: 22
                    leftPadding: 22
                    maximumLength: 510
                    placeholderText: qsTr("Password")
                    placeholderTextColor: Qt.rgba(1.0, 1.0, 1.0, 0.6)
                    color: palette.windowText
                    font.pixelSize: 12
                    onContentWidthChanged: updateLeftPadding()
                    onWidthChanged: updateLeftPadding()
                    Component.onCompleted: updateLeftPadding()

                    property bool capsIndicatorVisible: false

                    function updateLeftPadding() {
                        var remaining = width - contentWidth - rightPadding
                        if (capsIndicator.visible)
                            leftPadding = rightPadding
                        else
                            leftPadding = Math.max(8, remaining > rightPadding ? rightPadding : remaining)
                    }

                    // Copied from src/plugins/lockscreen/qml/UserInput.qml.
                    // Tests drive opacity; keep the 600ms timer off (interval 0).
                    cursorDelegate: Rectangle {
                        id: cursor
                        objectName: "lockCaret"
                        width: 1
                        height: 18
                        color: palette.windowText
                        visible: parent.activeFocus && !parent.readOnly
                                 && parent.selectionStart === parent.selectionEnd
                        Component.onCompleted: root.lockCaretItem = cursor

                        Connections {
                            target: cursor.parent
                            function onCursorPositionChanged() {
                                cursor.opacity = 1
                                if (cursorTimer.interval != 0)
                                    cursorTimer.restart()
                            }
                        }

                        Timer {
                            id: cursorTimer
                            running: cursor.parent.activeFocus && !cursor.parent.readOnly && interval != 0
                            repeat: true
                            interval: 0
                            onTriggered: cursor.opacity = !cursor.opacity ? 1 : 0
                            onRunningChanged: cursor.opacity = 1
                        }
                    }

                    Item {
                        id: capsIndicator
                        height: parent.height
                        anchors {
                            left: parent.left
                            leftMargin: 3
                            verticalCenter: parent.verticalCenter
                        }
                        visible: passwordField.capsIndicatorVisible
                        onVisibleChanged: passwordField.updateLeftPadding()
                        implicitWidth: 16
                        implicitHeight: 16
                    }

                    Button {
                        id: showPasswordBtn
                        anchors {
                            right: parent.right
                            rightMargin: 3
                            verticalCenter: parent.verticalCenter
                        }
                        property bool hiddenPWD: true
                        implicitWidth: 16
                        implicitHeight: 16
                        padding: 0
                        hoverEnabled: true
                        display: AbstractButton.TextOnly
                        text: hiddenPWD ? "·" : "o"

                        background: Rectangle {
                            anchors.fill: parent
                            radius: 4
                            color: showPasswordBtn.hovered ? Qt.rgba(0, 0, 0, 0.1) : "transparent"
                        }

                        onClicked: hiddenPWD = !hiddenPWD
                    }

                    background: RoundBlur {
                        id: passwordBlur
                        objectName: "lockPasswordBlur"
                        color: Qt.rgba(1, 1, 1, 0.4)
                        radius: 6
                    }
                }
            }

        Button {
            id: loginBtn
            objectName: "lockLoginBtn"
            display: AbstractButton.TextOnly
            text: ">"
            padding: 0
            height: passwordField.height
            width: height
            anchors {
                left: userCol.right
                bottom: userCol.bottom
                leftMargin: 20
            }
            enabled: passwordField.length != 0
            font.pixelSize: 12
            background: RoundBlur {
                anchors.fill: parent
                color: Qt.rgba(1.0, 1.0, 1.0, 0.4)
                radius: parent.height / 2
            }
        }
    }

        // Always in lockContent so updatePaintNode runs during settle, like
        // the password blitter. A hideable Item never put this node in the
        // output batches (no extra-QRhi copy, no skip log).
        RoundBlur {
            id: alwaysGlass
            objectName: "lockAlwaysGlass"
            x: 36
            y: 80
            width: 220
            height: 280
            z: 1
            color: Qt.rgba(1, 1, 1, 0.08)
            radius: 12
            transformOrigin: Item.Bottom
        }

        // ControlAction session chip, bottom-right. Expand copies RoundBlur
        // like the real 150ms width/height Behavior.
        Button {
            id: sessionButton
            objectName: "lockSessionButton"
            property bool expand: false
            width: expand ? 94 : 88
            height: expand ? 36 : 30
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 30
            anchors.bottomMargin: 30
            text: "Treeland"
            palette.buttonText: "white"
            Behavior on width { NumberAnimation { duration: 150 } }
            Behavior on height { NumberAnimation { duration: 150 } }

            background: RoundBlur {
                id: sessionBtnBlur
                objectName: "lockSessionBtnBlur"
                color: Qt.rgba(1, 1, 1, 0.3)
                radius: sessionButton.height / 2
            }

            onClicked: sessionPopup.open()
        }

        Button {
            id: userButton
            objectName: "lockUserButton"
            property bool expand: false
            width: expand ? 36 : 30
            height: expand ? 36 : 30
            anchors.right: sessionButton.left
            anchors.bottom: parent.bottom
            anchors.rightMargin: 15
            anchors.bottomMargin: 30
            Behavior on width { NumberAnimation { duration: 150 } }
            Behavior on height { NumberAnimation { duration: 150 } }

            background: RoundBlur {
                objectName: "lockUserBtnBlur"
                color: Qt.rgba(1, 1, 1, 0.3)
                radius: userButton.height / 2
            }
        }
    }

    Loader {
        id: loginHideLoader
        active: loginHideSource
        sourceComponent: ShaderEffectSource {
            live: true
            hideSource: true
            visible: true
            sourceItem: lockContent
            width: lockContent.width
            height: lockContent.height
            x: lockContent.x
            y: lockContent.y
        }
    }

    Item {
        id: sessionPopup
        objectName: "lockSessionPopup"
        visible: false
        width: 220
        height: 280
        transformOrigin: Item.Bottom
        layer.enabled: false
        x: Math.max(8, Math.min(sessionButton.x + (sessionButton.width - width) / 2,
                                root.width - width - 8))
        y: sessionButton.y - height - 10

        function open() {
            visible = true
            sessionButton.expand = true
            userButton.expand = true
        }
        function close() {
            visible = false
            sessionButton.expand = false
            userButton.expand = false
        }

        RoundBlur {
            id: sessionGlass
            objectName: "lockSessionGlass"
            anchors.fill: parent
            color: Qt.rgba(1, 1, 1, 0.08)
            radius: 12
        }

        ListView {
            id: sessionList
            objectName: "lockSessionList"
            anchors.fill: parent
            anchors.margins: 10
            clip: true
            spacing: 1
            model: [
                "Sway", "Treeland User", "Weston", "deepin",
                "GNOME", "KDE", "Xfce", "LXQt",
                "i3", "Hyprland", "labwc", "Cage"
            ]
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AlwaysOn
            }
            delegate: Item {
                required property string modelData
                width: sessionList.width
                height: 60
                Rectangle {
                    anchors.fill: parent
                    radius: 15
                    color: Qt.rgba(0, 0, 0, 0.05)
                }
                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5
                    Rectangle {
                        width: 36
                        height: 36
                        radius: 6
                        color: Qt.rgba(1, 1, 1, 0.85)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: modelData
                        color: "white"
                        width: sessionList.width - 60
                        elide: Text.ElideRight
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    function openSessionPopup() {
        popupEnterAnim.stop()
        popupExitAnim.stop()
        if (popupEnterAnimation) {
            sessionPopup.opacity = 0
            sessionPopup.scale = 0.85
            sessionPopup.open()
            popupEnterAnim.start()
        } else {
            sessionPopup.opacity = 1
            sessionPopup.scale = 1
            sessionPopup.open()
        }
    }

    function closeSessionPopup() {
        popupEnterAnim.stop()
        if (popupEnterAnimation && sessionPopup.visible) {
            popupExitAnim.restart()
        } else {
            popupExitAnim.stop()
            sessionPopup.opacity = 1
            sessionPopup.scale = 1
            sessionPopup.close()
        }
    }

    ParallelAnimation {
        id: popupEnterAnim
        NumberAnimation { target: sessionPopup; property: "opacity"; to: 1; duration: 240 }
        NumberAnimation { target: sessionPopup; property: "scale"; to: 1; duration: 240 }
    }
    ParallelAnimation {
        id: popupExitAnim
        NumberAnimation { target: sessionPopup; property: "opacity"; to: 0; duration: 240 }
        NumberAnimation { target: sessionPopup; property: "scale"; to: 0.85; duration: 240 }
        onFinished: {
            sessionPopup.close()
            sessionPopup.opacity = 1
            sessionPopup.scale = 1
        }
    }

    function scrollSessionList(dy) {
        sessionList.contentY += dy
    }

    function blinkCaret() {
        if (lockCaretItem)
            lockCaretItem.opacity = lockCaretItem.opacity > 0.5 ? 0 : 1
    }

    function startHideSource() {
        loginHideSource = true
    }

    function stopHideSource() {
        // Unhide before destroying the ShaderEffectSource. If the Loader
        // drops the effect while hideSource is still true, lockContent stays
        // hidden and chip-vs-rerender compares wallpaper to wallpaper.
        var fx = loginHideLoader.item
        if (fx) {
            fx.hideSource = false
            fx.sourceItem = null
        }
        loginHideSource = false
        lockContent.visible = true
    }

    function resetLock() {
        stopHideSource()
        popupEnterAnimation = false
        popupEnterAnim.stop()
        popupExitAnim.stop()
        sessionPopup.opacity = 1
        sessionPopup.scale = 1
        sessionPopup.close()
        sessionList.contentY = 0
        if (lockCaretItem)
            lockCaretItem.opacity = 1
        passwordField.text = ""
        passwordField.focus = false
    }
}

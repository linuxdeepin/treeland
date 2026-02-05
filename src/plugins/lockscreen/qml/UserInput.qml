// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.deepin.dtk 1.0 as D
import Treeland
import LockScreen

Item {
    id: loginGroup
    width: 220
    height: 300

    property string normalHint: qsTr("Please enter password")

    /**************/
    /* Components */
    /**************/

    Item {
        width: 32
        height: 32
        visible: GreeterProxy.isLocked
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.top
            bottomMargin: 56
        }
        D.DciIcon {
            name: "login_lock"
            anchors.centerIn: parent
            sourceSize {
                width: 18
                height: 22
            }
        }
    }

    Column {
        id: userCol
        spacing: 15
        anchors.centerIn: parent

        Rectangle {
            width: 120
            height: 120
            anchors.horizontalCenter: parent.horizontalCenter
            color: "transparent"
            radius: 20
            border {
                width: 2
                color: Qt.rgba(1, 1, 1, 0.1)
            }

            D.QtIcon {
                id: avatar
                anchors.fill: parent
                visible: false
            }

            Rectangle {
                id: maskSource
                radius: 20
                anchors.fill: avatar
                visible: false
            }
            D.OpacityMask {
                anchors.fill: avatar
                source: avatar
                maskSource: maskSource
                invert: false
            }
            Rectangle {
                radius: 20
                anchors.fill: avatar
                color: "transparent"
                border {
                    width: 1
                    color: Qt.rgba(0, 0, 0, 0.2)
                }
            }
        }

        Text {
            text: "User"
            id: username
            font.bold: true
            font.pixelSize: D.DTK.fontManager.t2.pixelSize
            font.family: D.DTK.fontManager.t2.family
            color: palette.windowText
            anchors.horizontalCenter: parent.horizontalCenter
        }

        TextField {
            id: passwordField

            property bool capsIndicatorVisible: false

            cursorDelegate: Rectangle {
                id: cursor

                width: 1
                height: 18
                color: palette.windowText

                visible: parent.activeFocus && !parent.readOnly && parent.selectionStart === parent.selectionEnd

                Connections {
                    target: cursor.parent
                    function onCursorPositionChanged() {
                        // keep a moving cursor visible
                        cursor.opacity = 1
                        cursorTimer.restart()
                    }
                }

                Timer {
                    id: cursorTimer
                    running: cursor.parent.activeFocus && !cursor.parent.readOnly && interval != 0
                    repeat: true
                    // TODO: Application.styleHints.cursorFlashTime / 2, waylib is not supports
                    // Application.styleHints now.
                    interval: 600
                    onTriggered: cursor.opacity = !cursor.opacity ? 1 : 0
                    // force the cursor visible when gaining focus
                    onRunningChanged: cursor.opacity = 1
                }
            }

            width: loginGroup.width
            height: 30
            anchors.horizontalCenter: parent.horizontalCenter
            echoMode: showPasswordBtn.hiddenPWD ? TextInput.Password : TextInput.Normal
            rightPadding: capsIndicator.visible ? 24 + capsIndicator.width : 24
            maximumLength: 510
            placeholderText: qsTr("Password")
            placeholderTextColor: Qt.rgba(1.0, 1.0, 1.0, 0.6)
            color: palette.windowText
            font: D.DTK.fontManager.t8
            Keys.onPressed: function (event) {
                if (event.key === Qt.Key_CapsLock) {
                    capsIndicatorVisible = !capsIndicatorVisible
                    event.accepted = true
                } else if (event.key === Qt.Key_Return) {
                    userLogin()
                    event.accepted = true
                }
            }

            RowLayout {
                height: parent.height
                anchors {
                    right: parent.right
                    rightMargin: 3
                }

                D.ActionButton {
                    id: capsIndicator
                    visible: passwordField.capsIndicatorVisible
                    palette.windowText: undefined
                    icon {
                        name: "login_capslock"
                        height: 10
                        width: 10
                    }
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 16
                    implicitHeight: 16
                }

                D.ActionButton {
                    id: showPasswordBtn
                    property bool hiddenPWD: true
                    icon {
                        name: hiddenPWD ? "login_display_password" : "login_hidden_password"
                        height: 10
                        width: 10
                    }
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 16
                    implicitHeight: 16
                    hoverEnabled: true

                    background: Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: showPasswordBtn.hovered ? Qt.rgba(
                                                            0, 0, 0,
                                                            0.1) : "transparent"
                    }

                    onClicked: hiddenPWD = !hiddenPWD
                }
            }

            background: RoundBlur {
                color: Qt.rgba(1, 1, 1, 0.4)
                radius: 6
            }
        }
    }

    D.ActionButton {
        id: loginBtn
        icon {
            name: "login_open"
            width: 16
            height: 16
        }
        height: passwordField.height
        width: height
        anchors {
            left: userCol.right
            bottom: userCol.bottom
            leftMargin: 20
        }
        enabled: passwordField.length != 0
        font: D.DTK.fontManager.t8
        background: RoundBlur {
            anchors.fill: parent
            color: Qt.rgba(1.0, 1.0, 1.0, 0.4)
            radius: parent.height / 2
        }

        onClicked: userLogin()
    }

    RowLayout {
        spacing: 10
        anchors {
            right: userCol.left
            bottom: userCol.bottom
            rightMargin: 10
        }
        height: passwordField.height

        D.RoundButton {
            id: langBtn
            text: 'L' // TODO: replace with icon
            visible: false
            Layout.fillHeight: true
            Layout.preferredWidth: passwordField.height
            background: Rectangle {
                anchors.fill: parent
                color: Qt.rgba(1.0, 1.0, 1.0, 0.4)
                radius: parent.height / 2
            }

            onClicked: {
                // TODO: impl language switch
                console.log("need impl langBtn")
            }
        }

        D.RoundButton {
            id: hintBtn
            icon {
                name: "login_hint"
                width: 10
                height: 10
            }
            visible: hintLabel.hintText.length != 0
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            background: Rectangle {
                id: hintBtnBackground
                visible: hintBtn.hovered
                anchors.fill: parent
                color: Qt.rgba(1.0, 1.0, 1.0, 0.1)
                radius: 4
            }

            HintLabel {
                id: hintLabel
                x: hintBtn.width - hintLabel.width
                y: hintBtn.height + 11
                hintText: {
                    let user = UserModel.get(UserModel.currentUserName)
                    return user.passwordHint
                }
            }

            onClicked: hintLabel.open()
        }
    }

    Text {
        id: hintText
        font: D.DTK.fontManager.t8
        color: Qt.rgba(1.0, 1.0, 1.0, 0.7)
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: userCol.bottom
            topMargin: 16
        }
    }

    /*****************************/
    /* Functions and Connections */
    /*****************************/

    function updateUser() {
        let currentUser = UserModel.get(UserModel.currentUserName)
        username.text = currentUser.realName.length === 0 ? currentUser.name : currentUser.realName
        passwordField.text = ''
        avatar.fallbackSource = currentUser.icon
        hintText.text = normalHint
    }

    function userLogin() {
        let user = UserModel.get(UserModel.currentUserName)
        if (user.loggedIn)
            GreeterProxy.unlock(user.name, passwordField.text)
        else
            GreeterProxy.login(user.name, passwordField.text, SessionModel.currentIndex)
    }

    Connections {
        target: GreeterProxy
        function onFailedAttemptsChanged (attempts) {
            if (attempts > 0) {
                passwordField.selectAll()
                if (loginGroup.activeFocus) {
                    passwordField.forceActiveFocus()
                }
                hintText.text = qsTr("Password is incorrect.")
            } else {
                passwordField.text = ""
                hintText.text = normalHint
            }
        }
    }

    Connections {
        target: UserModel

        function onUpdateTranslations(locale) {
            updateUser()
        }

        function onCurrentUserNameChanged(name) {
            updateUser()
        }
    }

    Component.onCompleted: {
        updateUser()
    }

    onActiveFocusChanged: {
        if (activeFocus) passwordField.forceActiveFocus()
    }
}

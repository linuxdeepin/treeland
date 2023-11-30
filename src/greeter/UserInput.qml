// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.deepin.dtk 1.0 as D
import TreeLand.Greeter

Item {
    id: loginGroup
    width: 220
    height: 300

    function updateUser() {
        let currentUser = GreeterModel.userModel.get(
                GreeterModel.currentUserIndex)
        username.text = currentUser.realName.length == 0 ? currentUser.name : currentUser.realName
        passwordField.text = ''
        avatar.source = currentUser.icon
        passwordField.visible = !currentUser.logined
        lockIcon.source = currentUser.logined ? "file:///usr/share/icons/Papirus-Dark/16x16/actions/unlock" : "file:///usr/share/icons/Papirus-Dark/16x16/actions/lock"
    }

    function userLogin() {
        let user = GreeterModel.userModel.get(GreeterModel.currentUserIndex)
        if (user.logined) {
            GreeterModel.proxy.unlock(user)
            return
        }

        if (passwordField.text.length === 0) {
            return
        }

        GreeterModel.proxy.login(user.name, passwordField.text,
                                 GreeterModel.currentSession)
    }

    Connections {
        target: GreeterModel.userModel
        function onDataChanged() {
            updateUser()
        }
    }

    Connections {
        target: GreeterModel
        function onCurrentUserIndexChanged() {
            updateUser()
        }
    }

    Connections {
        target: Greeter
        function onVisibleChanged(visible) {
            if (!visible) {
                passwordField.text = ''
            }
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: 24
        height: 24
        color: "transparent"
        anchors.top: parent.top
        anchors.topMargin: 10
        Image {
            id: lockIcon
            anchors.fill: parent
            height: 24
            width: 24
            fillMode: Image.PreserveAspectFit
        }
    }

    Column {
        id: userCol
        spacing: 15
        anchors.centerIn: parent

        Item {
            width: 120
            height: 120
            anchors.horizontalCenter: parent.horizontalCenter
            Image {
                id: avatar
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
            }
        }

        Text {
            text: "User"
            id: username
            font.bold: true
            font.pointSize: 15
            color: "white"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        TextField {
            id: passwordField
            width: loginGroup.width
            height: 30
            anchors.horizontalCenter: parent.horizontalCenter
            echoMode: TextInput.Password
            focus: true
            rightPadding: 24
            onAccepted: userLogin()
            RoundButton {
                id: showPasswordBtn
                property var showPassword: false

                text: 'P'
                width: 24
                height: 24
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 2
                background: Rectangle {
                    id: hintButtonBackground
                    anchors.fill: parent
                    radius: parent.height / 2
                    color: "transparent"
                }

                onClicked: {
                    showPassword = !showPassword
                    passwordField.echoMode = showPassword ? TextInput.Normal : TextInput.Password
                }

                onPressed: {
                    hintButtonBackground.color = Qt.rgba(0, 0, 0, 0.6)
                }

                onReleased: {
                    hintButtonBackground.color = "transparent"
                }
            }

            background: Rectangle {
                implicitWidth: parent.width
                implicitHeight: parent.height
                color: Qt.rgba(1, 1, 1, 0.6)
                radius: 30
            }
        }
    }

    RoundButton {
        id: loginBtn
        text: "\u2192"
        height: passwordField.height
        width: height
        anchors.left: userCol.right
        anchors.bottom: userCol.bottom
        anchors.leftMargin: 10
        background: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(104 / 255, 158 / 255, 233 / 255, 0.5)
            radius: parent.height / 2
        }

        onClicked: userLogin()
    }

    RoundButton {
        property var showHint: false

        id: hintBtn
        text: 'H'
        height: passwordField.height
        width: height
        anchors.right: userCol.left
        anchors.bottom: userCol.bottom
        anchors.rightMargin: 10
        background: Rectangle {
            id: hintBtnBackground
            anchors.fill: parent
            color: Qt.rgba(104 / 255, 158 / 255, 233 / 255, 0.5)
            radius: parent.height / 2
        }

        onClicked: {
            showHint = !showHint
        }

        onPressed: {
            hintBtnBackground.color = Qt.rgba(0, 0, 0, 0.6)
        }

        onReleased: {
            hintBtnBackground.color = Qt.rgba(104 / 255, 158 / 255,
                                              233 / 255, 0.5)
        }
    }

    RoundButton {
        id: langBtn
        text: 'L'
        height: hintBtn.height
        width: height
        anchors.right: hintBtn.left
        anchors.bottom: hintBtn.bottom
        anchors.rightMargin: 10
        background: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(104 / 255, 158 / 255, 233 / 255, 0.5)
            radius: parent.height / 2
        }

        onClicked: {
            console.log("need impl langBtn")
        }
    }

    Text {
        text: hintBtn.showHint ? "This is user custom hint message." : "Please enter your password or fingerprint."
        id: hintText
        font.pointSize: 10
        color: "gray"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: userCol.bottom
        anchors.topMargin: 10
    }

    Component.onCompleted: {
        updateUser()
    }
}

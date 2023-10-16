import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import TreeLand.Greeter

Item {
    width: 260
    height: 210
    Rectangle {
        color: Qt.rgba(1, 1, 1, 0.5)
        anchors.fill: parent
        anchors.topMargin: 40
        radius: 15
    }

    function updateUser() {
        username.text = GreeterModel.currentUser
        password.text = ''
        avatar.source = GreeterModel.userModel.get(GreeterModel.currentUser).icon

        var user = GreeterModel.userModel.get(GreeterModel.currentUser);
        password.visible = !user.logined
    }

    Connections {
        target: GreeterModel.userModel
        onDataChanged: {
            updateUser()
        }
    }

    Connections {
        target: GreeterModel
        function onCurrentUserChanged() {
            updateUser()
        }
    }

    Connections {
        target: GreeterModel.proxy
        function onVisibleChanged(visible) {
            if (!visible) {
                password.text = ''
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        Rectangle {
            width: 80
            height: 80
            Layout.alignment: Qt.AlignHCenter
            Image {
                id: avatar
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
            }
        }
        Text {
            text: "User"
            id: username
            Layout.alignment: Qt.AlignHCenter
        }

        TextField {
            id: password
            width: 255
            height: 30
            Layout.alignment: Qt.AlignHCenter
            echoMode: TextInput.Password
            focus: true
        }
        Button {
            text: "Login"
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                var user = GreeterModel.userModel.get(GreeterModel.currentUser);
                if (user.logined) {
                    // TODO: hide greeter
                    root.visible = false
                    return;
                }
                GreeterModel.worker.createAuthentication(GreeterModel.currentUser)
                GreeterModel.worker.sendTokenToAuth(GreeterModel.currentUser, 1, password.text)
                //GreeterModel.proxy.login(GreeterModel.currentUser, password.text, GreeterModel.currentSession);
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }

    Component.onCompleted: {
        updateUser()
    }
}

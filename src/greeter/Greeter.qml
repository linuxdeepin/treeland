import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import TreeLand.Greeter

Item {
    id: root

    function onCurrentUserChanged() {
        var user = GreeterModel.userModel.get(GreeterModel.currentUser);
        //sessionBtn.visible = !user.logined;
    }

    Connections {
        target: GreeterModel
        function onCurrentUserChanged() {
          root.onCurrentUserChanged()
        }
    }

    Connections {
        target: GreeterModel.proxy
        function onVisibleChanged(visible) {
            root.visible = visible
            root.onCurrentUserChanged();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        Loader {
            id: center
            Layout.alignment: Qt.AlignHCenter
        }
        RowLayout {
            id: bottom
            Item {
                Layout.fillWidth: true
            }
            Row {
                id: button
                Layout.rightMargin: 150
                spacing: 10
                //Button {
                //    id: sessionBtn
                //    text: "Session"
                //    SessionList {
                //      id: sessionList
                //    }
                //    onClicked: {
                //        sessionList.x = (width - sessionList.width) / 2
                //        sessionList.y = - sessionList.height
                //        sessionList.open()
                //    }
                //}
                Button {
                    id: userBtn
                    text: "User"
                    UserList {
                      id: userList
                    }
                    onClicked: {
                        userList.x = (width - userList.width) / 2
                        userList.y = - userList.height
                        userList.open()
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        center.source = "UserInput.qml"
    }
}

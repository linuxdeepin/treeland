// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    Loader {
        id: leftAnimation
    }

    Component {
        id: leftAnimationComponent

        LoginAnimation {
            onStopped: {
            }
        }
    }

    Loader {
        id: rightAnimation
    }

    Component {
        id: rightAnimationComponent

        LoginAnimation {
            onStopped: {
                GreeterModel.animationPlayFinished()
            }
        }
    }

    Connections {
        target: GreeterModel
        function onAnimationPlayed() {
            leftAnimation.parent = quickActions.parent
            leftAnimation.anchors.fill = quickActions
            leftAnimation.sourceComponent = leftAnimationComponent
            leftAnimation.item.start(quickActions,{x: quickActions.x}, {x: -quickActions.width})

            rightAnimation.parent = right.parent
            rightAnimation.anchors.fill = right
            rightAnimation.sourceComponent = rightAnimationComponent
            rightAnimation.item.start(right, {x: 0}, {x: right.width})
        }
    }

    Item {
        id: quickActions
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.width * 0.4
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 30
            anchors.verticalCenter: parent.verticalCenter
            width: 250
            height: 300
            color: Qt.rgba(1, 1, 1, 0.5)
            radius: 15
            Text {
                text: "There will display some widgets."
            }
        }

        Rectangle {
            width: 50
            height: 30
            color: 'red'
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            visible: false
        }
    }

    Item {
        id: right
        anchors.left: quickActions.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        UserInput {
            id: userInput
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 60
        }
        RowLayout {
            id: bottom
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            Item {
                Layout.fillWidth: true
            }
            Row {
                id: button
                spacing: 10
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
}


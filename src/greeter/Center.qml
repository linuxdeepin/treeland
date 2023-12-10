// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import org.deepin.dtk 1.0 as D

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

        TimeDateWidget {
            anchors.right: parent.right
            anchors.rightMargin: parent.width / 10
            anchors.verticalCenter: parent.verticalCenter
            width: 400
            height: 156
            background: Rectangle {
                radius: 8
                color: "white"
                opacity: 0.1
            }
        }
    }

    Rectangle {
        id: logo
        width: 100
        height: 60
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        color: Qt.rgba(1, 1, 1, 0.5)
        Text {
            width: parent.width
            height: parent.height
            text: "This is system logo."
            color: 'black'
            wrapMode: Text.Wrap
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
            anchors.leftMargin: 80
        }

        RowLayout {
            id: bottomGroup
            property var buttonSize: 30
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            spacing: 15

            Rectangle {
                implicitWidth: bottomGroup.buttonSize + 6
                implicitHeight: bottomGroup.buttonSize + 6
                color: "transparent"
                Layout.alignment: Qt.AlignHCenter
                RoundButton {
                    id: wifiBtn
                    text: '1'
                    width: bottomGroup.buttonSize
                    height: bottomGroup.buttonSize
                    anchors.centerIn: parent
                    onClicked: {
                        console.log("need impl btn 1")
                    }

                    background: RoundBlur {}
                }
            }

            Rectangle {
                implicitWidth: bottomGroup.buttonSize + 6
                implicitHeight: bottomGroup.buttonSize + 6
                color: "transparent"
                Layout.alignment: Qt.AlignHCenter
                RoundButton {
                    id: boardBtn
                    text: '2'
                    width: bottomGroup.buttonSize
                    height: bottomGroup.buttonSize
                    anchors.centerIn: parent
                    onClicked: {
                        console.log(userList.count > 1)
                    }

                    background: RoundBlur {}
                }
            }

            Rectangle {
                implicitWidth: bottomGroup.buttonSize + 6
                implicitHeight: bottomGroup.buttonSize + 6
                color: "transparent"
                Layout.alignment: Qt.AlignHCenter
                visible: userList.count > 1

                RoundButton {
                    id: usersBtn
                    text: '3'
                    width: bottomGroup.buttonSize
                    height: bottomGroup.buttonSize
                    anchors.centerIn: parent
                    hoverEnabled: parent.visible
                    D.ToolTip.visible: hovered
                    D.ToolTip.text: qsTr("Other Users")
                    UserList {
                        id: userList
                        onClosed: {
                            usersBtn.width -= 6
                            usersBtn.height -= 6
                        }
                    }

                    onClicked: {
                        usersBtn.width += 6
                        usersBtn.height += 6
                        userList.x = (width - userList.width) / 2 - 10
                        userList.y = -userList.height - 10
                        userList.open()
                    }

                    background: RoundBlur {}
                }
            }

            Rectangle {
                implicitWidth: bottomGroup.buttonSize + 6
                implicitHeight: bottomGroup.buttonSize + 6
                color: "transparent"
                Layout.alignment: Qt.AlignHCenter
                RoundButton {
                    id: powerBtn
                    text: '4'
                    width: bottomGroup.buttonSize
                    height: bottomGroup.buttonSize
                    anchors.centerIn: parent
                    D.ToolTip.visible: hovered
                    D.ToolTip.text: qsTr("Power")
                    PowerList {
                        id: powerList
                        onClosed: {
                            powerBtn.width -= 6
                            powerBtn.height -= 6
                        }
                    }
                    onClicked: {
                        powerBtn.width += 6
                        powerBtn.height += 6
                        powerList.x = (width - powerList.width) / 2 - 10
                        powerList.y = -powerList.height - 10
                        powerList.open()
                    }

                    background: RoundBlur {}
                }
            }
        }
    }
}

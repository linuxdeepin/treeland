// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Treeland
import LockScreen

Popup {
    id: popup
    modal: true
    width: 220
    height: 280
    background: RoundBlur {
        radius: 12
    }

    function updateCurrentSession(index) {
        GreeterModel.currentSession = index
    }

    ListView {
        id: list
        spacing: 1
        width: parent.width - 20
        height: parent.height - 20
        x: 10
        y: 10
        clip: true
        model: SessionModel

        ScrollBar.vertical: ScrollBar {
            id: verticalScrollBar
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            policy: ScrollBar.AlwaysOn
        }

        delegate: Item {
            required property string name
            required property int index
            width: parent.width
            height: 60
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true
                onEntered: {
                    background.color = Qt.rgba(0, 129/255, 255/255, 0.9)
                }
                onExited: {
                    background.color = Qt.rgba(0, 0, 0, 0.05)
                }
                onClicked: (mouse) => {
                    mouse.accepted = false
                    updateCurrentSession(index)
                    popup.close()
                }
            }

            Rectangle {
                id: background
                width: parent.width
                height: parent.height
                color: Qt.rgba(0, 0, 0, 0.05)
                radius: 15
            }
            RowLayout {
                spacing: 5
                anchors.fill: parent
                Rectangle {
                    width: 36
                    height: 36
                    Layout.leftMargin: 5
                    Image {
                        anchors.fill: parent
                        source: name
                        fillMode: Image.PreserveAspectFit
                    }
                }
                Text {
                    text: name
                    Layout.fillWidth: true
                }
                Image {
                    // logined
                }
            }
        }
    }
    Component.onCompleted: {
        list.currentIndex = GreeterModel.currentSession
    }
}


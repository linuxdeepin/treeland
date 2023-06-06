// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick.Controls 2.5
import QtQuick 2.0
import Qt.labs.platform 1.0

ApplicationWindow {
    id: window
    x: 50
    y: 50
    width: 300
    height: 300
    visible: true

    Rectangle {
        anchors.fill: parent
        radius: 20

        property alias animationRunning: ani.running

        gradient: Gradient {
            GradientStop { position: 0; color: "steelblue" }
            GradientStop { position: 1; color: "yellow" }
        }

        Text {
            anchors.centerIn: parent
            text: "Qt Quick in a texture"
            font.pointSize: 40
            color: "white"

            SequentialAnimation on rotation {
                id: ani
                running: true
                PauseAnimation { duration: 1500 }
                NumberAnimation { from: 0; to: 360; duration: 5000; easing.type: Easing.InOutCubic }
                loops: Animation.Infinite
            }
        }

        Column {
            anchors {
                right: parent.right
                bottom: parent.bottom
                margins: 50
            }

            spacing: 10

            Button {
                text: "Quit"
                onClicked: {
                    Qt.quit()
                }
            }
        }
    }

    TextField {
        anchors.horizontalCenter: parent.horizontalCenter
        text: "I am a Qt window"
        font.pointSize: 20
    }

    Menu {
        id: menu

        MenuItem {
            text: "Test 1"
        }

        Menu {
            title: "Sub Menu"
            MenuItem {
                text: "Test 2"
            }
        }
    }

    Button {
        id: menuButton
        text: "Menu"
        anchors.centerIn: parent
        onClicked: {
            menu.open(menuButton)
        }
    }
}

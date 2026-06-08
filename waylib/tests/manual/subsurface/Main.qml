// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import subsurface

Window {
    id: root
    width: 400
    height: 400
    visible: true
    title: qsTr("Wayland Subsurface Test")

    CustomWindow {
        parent: root
        title: "Subsurface 1"
        x: -50
        width: 100
        height: 100
        color: "red"
    }

    CustomWindow {
        id: window2

        parent: root
        title: "Subsurface 2"

        width: 100
        height: 100
        color: "blue"

        MouseArea {
            property point windowLastPosition
            property point pressedPosition

            anchors.fill: parent
            onPressed: function(event) {
                pressedPosition = Qt.point(event.x, event.y)
                windowLastPosition = Qt.point(window2.x, window2.y)
            }

            onPositionChanged: function(event) {
                if (pressed) {
                    window2.x = windowLastPosition.x + event.x - pressedPosition.x
                    window2.y = windowLastPosition.y + event.y - pressedPosition.y
                }
            }
        }
    }

    CustomWindow {
        parent: root
        title: "Subsurface 3"

        x: 200
        y: 100
        width: 200
        height: 200
        color: "yellow"

        Column {
            anchors.centerIn: parent
            property CustomWindow winObj
            Button {
                text: "Add"
                onClicked: {
                    parent.winObj = window4.createObject()
                }
            }
            Button {
                text: "Destroy"
                onClicked: {
                    console.log('destroying',parent.winObj,parent.winObj.close())
                }
            }
        }

    }

    Component {
        id: window4

        CustomWindow {
            parent: root
            title: "Subsurface 4"

            x: 300
            y: -50
            width: 150
            height: 100
            color: "green"
        }
    }
}

// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import pinchhandler

Window {
    width: 1000
    height: 800
    visible: true
    title: handler.persistentRotation.toFixed(1) + "° " +
           handler.persistentTranslation.x.toFixed(1) + ", " +
           handler.persistentTranslation.y.toFixed(1) + " " +
           (handler.persistentScale * 100).toFixed(1) + "%"

    Flickable {
        id: flickable
        anchors.fill: parent
        contentWidth: map.width * handler.persistentScale
        contentHeight: map.height * handler.persistentScale
        clip: true

        Rectangle {
            id: map
            color: "aqua"
            width: 1000
            height: 1000

            border.width: 1
            border.color: "black"
            gradient: Gradient {
                GradientStop { position: 0.0; color: "blue" }
                GradientStop { position: 1.0; color: "green" }
            }
            Text {
                text: qsTr("This is a PinchHandler Demo!")
                color: "white"
                anchors.centerIn: parent
            }
        }
    }

    PinchHandler {
        id: handler
        target: map
        onScaleChanged: {
            flickable.contentWidth = map.width * handler.persistentScale
            flickable.contentHeight = map.height * handler.persistentScale
        }
        onTranslationChanged: {
            flickable.contentX = handler.persistentTranslation.x
            flickable.contentY = handler.persistentTranslation.y
        }
        onActiveChanged: {
            if (!active) {
                flickable.returnToBounds()
            }
        }
    }

    EventItem {
        id: eventItem
        anchors.fill: parent
    }

}

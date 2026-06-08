// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import pinchhandler

Window {
    width: 1000; height: 800
    visible: true
    title: handler.persistentRotation.toFixed(1) + "° " +
           handler.persistentTranslation.x.toFixed(1) + ", " +
           handler.persistentTranslation.y.toFixed(1) + " " +
           (handler.persistentScale * 100).toFixed(1) + "%"

    Rectangle {
        id: map
        color: "aqua"
        x: (parent.width - width) / 2;
        y: (parent.height - height) / 2;
        width: 400
        height: 300

        border.width: 1
        border.color: "black"
        gradient: Gradient {
            GradientStop {  position: 0.0;    color: "blue"  }
            GradientStop {  position: 1.0;    color: "green" }
        }
        Text {
            text:             qsTr("This is a PinchHandler Demo!")
            color:            "white"
            anchors.centerIn: parent
        }
    }

    EventItem {
        id: eventItem
        anchors.fill: parent
    }

    PinchHandler {
        id: handler
        target: map
    }
}

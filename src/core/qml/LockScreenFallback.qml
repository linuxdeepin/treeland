// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick 2.15

FocusScope {
    id: root
    clip: true
    required property QtObject outputItem
    
    // Position and size
    x: outputItem.x
    y: outputItem.y
    width: outputItem.width
    height: outputItem.height
    
    Component.onCompleted: forceActiveFocus()
    
    Keys.onPressed: event => event.accepted = true
    Keys.onReleased: event => event.accepted = true
    
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: mouse => { mouse.accepted = true; root.forceActiveFocus() }
        onReleased: mouse => mouse.accepted = true
        onClicked: mouse => mouse.accepted = true
        onWheel: wheel => wheel.accepted = true
    }
    
    MultiPointTouchArea {
        anchors.fill: parent
        onPressed: touchPoints => root.forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: "grey"
    }
}

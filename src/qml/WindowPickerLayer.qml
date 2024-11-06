// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland

WindowPicker {
    anchors.fill: parent
    id: root
    Rectangle {
        x: selectionRegion.x
        y: selectionRegion.y
        width: selectionRegion.width
        height: selectionRegion.height
        color: "transparent"
        border.color: "red"
        border.width: 1
    }

    Text {
        anchors.fill: parent
        font.pixelSize: 50
        color: "red"
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        text: hint
    }
}

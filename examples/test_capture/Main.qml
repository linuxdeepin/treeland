// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import capture


CanvasWindow {
    id: canvas
    width: 600
    height: 400
    visible: true
    color: "transparent"
    Image {
        id: watermark
        source: "qrc:/watermark.png"
        sourceSize: Qt.size(100, 100)
        fillMode: Image.Tile
        anchors.fill: parent
    }
    ToolWindow {
        parent: canvas
        width: 100
        height: 30
        visible: true
        color: "transparent"
        Button {
            width: 100
            height: 30
            text: "Toggle watermark"
        }
    }
}

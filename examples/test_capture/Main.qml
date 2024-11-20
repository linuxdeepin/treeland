// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import capture

Window {
    id: canvas
    width: 600
    height: 400
    visible: true
    color: "transparent"
    flags: Qt.WindowTransparentForInput
    Image {
        id: watermark
        visible: false
        source: "qrc:/watermark.png"
        sourceSize: Qt.size(100, 100)
        fillMode: Image.Tile
        x: TreelandCaptureManager.context?.captureRegion.x ?? 0
        y: TreelandCaptureManager.context?.captureRegion.y ?? 0
        width: TreelandCaptureManager.context?.captureRegion.width ?? 0
        height: TreelandCaptureManager.context?.captureRegion.height ?? 0
    }
    SubWindow {
        x: TreelandCaptureManager.context?.captureRegion.x ?? 0
        y: Math.min(TreelandCaptureManager.context?.captureRegion.bottom ?? 0, canvas.height - 2 * height)
        id: toolBar
        parent: canvas
        width: row.width
        height: row.height
        visible: true
        color: "transparent"
        Row {
            id: row
            Button {
                width: 100
                height: 30
                text: watermark.visible ? "Hide watermark" : "Show watermark"
                onClicked: {
                    watermark.visible = !watermark.visible
                }
            }
            Button {
                width: 100
                height: 30
                text: TreelandCaptureManager.record ? "Screenshot" : "Record"
                onClicked: {
                    TreelandCaptureManager.record = !TreelandCaptureManager.record
                }
            }
            Button {
                width: 100
                height: 30
                text: "Finish"
                onClicked: {
                    TreelandCaptureManager.finishSelect()
                }
            }
        }
    }
    Window {
        id: playerWindow
        visible: TreelandCaptureManager.recordStarted
        width: player.width
        height: player.height
        color: "black"
        Player {
            id: player
            width: 100
            height: 100
            captureContext: TreelandCaptureManager.context
        }
    }
}

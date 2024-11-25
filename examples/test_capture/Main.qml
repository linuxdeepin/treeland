// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import capture

Window {
    id: canvas
    width: 600
    height: 400
    visible: !TreelandCaptureManager.recordStarted
    color: "transparent"
    flags: regionReady ? 0 : Qt.WindowTransparentForInput
    property rect captureRegion: TreelandCaptureManager.context?.captureRegion ?? Qt.rect(0, 0, 0, 0)
    property bool regionReady: captureRegion.width !== 0 && captureRegion.height !== 0

    Image {
        id: watermark
        visible: false
        source: "qrc:/watermark.png"
        sourceSize: Qt.size(100, 100)
        fillMode: Image.Tile
        x: captureRegion.x
        y: captureRegion.y
        width: captureRegion.width
        height: captureRegion.height
    }

    SubWindow {
        id: toolBar
        parent: canvas
        visible: regionReady
        x: captureRegion.x
        y: Math.min(captureRegion.bottom, canvas.height - 2 * height)
        width: row.width
        height: row.height
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

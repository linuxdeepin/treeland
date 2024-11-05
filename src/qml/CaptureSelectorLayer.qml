// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland.Capture

CaptureSourceSelector {
    id: captureSourceSelector
    anchors.fill: parent
    Rectangle {
        anchors.fill: parent
        color: "black"
        opacity: 0.05
    }

    Rectangle {
        x: selectionRegion.x
        y: selectionRegion.y
        width: selectionRegion.width
        height: selectionRegion.height
        color: "transparent"
        border.color: "red"
        border.width: 1
    }

}

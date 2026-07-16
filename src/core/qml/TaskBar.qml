// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma ComponentBehavior: Bound

import QtQuick
import Waylib.Server
import Treeland

ListView {
    required property QtObject output // Treeland Output (QML_ANONYMOUS), cannot use as named type
    readonly property OutputItem outputItem: output.outputItem

    width: 250 + leftMargin + rightMargin
    height: Math.min(outputItem.height, contentHeight) + topMargin + bottomMargin
    x: outputItem.x
    y: (outputItem.height - height) / 2
    spacing: 80
    leftMargin: 40
    rightMargin: 40
    topMargin: 40
    bottomMargin: 40
    model: output.minimizedSurfaces
    delegate: Item {
        required property SurfaceWrapper surface

        width: proxy.width
        height: proxy.height

        SurfaceProxy {
            id: proxy
            live: true
            surface: parent.surface
            maxSize: Qt.size(250, 150)
        }

        MouseArea {
            anchors.fill: parent
            onClicked: parent.surface.restoreFromMinimized()
        }
    }

    transform: Rotation {
        angle: 30
        axis {
            x: 0
            y: 1
            z: 0
        }

        origin {
            x: width / 2
            y: height / 2
        }
    }

    layer {
        enabled: true
        live: true
        mipmap: true
        smooth: true
    }
}

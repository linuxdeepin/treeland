// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import Treeland

Item {
    id: root

    required property SurfaceWrapper surface

    property real radius: 0
    property color insideColor: Qt.rgba(255, 255, 255, 0.1)

    readonly property int outsideWidth: surface.border.width
    readonly property color outsideColor: surface.border.color

    Rectangle {
        id: outsideBorder
        visible: outsideWidth > 0
        anchors {
            fill: parent
            margins: -outsideWidth
        }

        color: "transparent"
        border {
            color: outsideColor
            width: outsideWidth
        }
        radius: GraphicsInfo.api === GraphicsInfo.Software ? 0 : root.radius + outsideWidth
    }

    Rectangle {
        id: insideBorder
        visible: outsideWidth > 0
        anchors.fill: parent
        color: "transparent"
        border {
            color: insideColor
            width: 1
        }
        radius: GraphicsInfo.api === GraphicsInfo.Software ? 0 : root.radius
    }
}

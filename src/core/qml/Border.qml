// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

Item {
    id: root

    property real radius: 0
    property color outsideColor: Qt.rgba(0, 0, 0, 0.1)
    property color insideColor: Qt.rgba(255, 255, 255, 0.1)

    Rectangle {
        id: outsideBorder
        anchors {
            fill: parent
            margins: -border.width
        }

        color: "transparent"
        border {
            color: outsideColor
            width: 1
        }
        radius: GraphicsInfo.api === GraphicsInfo.Software ? 0 : root.radius + border.width
    }

    Rectangle {
        id: insideBorder
        anchors.fill: parent
        color: "transparent"
        border {
            color: insideColor
            width: 1
        }
        radius: GraphicsInfo.api === GraphicsInfo.Software ? 0 : root.radius
    }
}

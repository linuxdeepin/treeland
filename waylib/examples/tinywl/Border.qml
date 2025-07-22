// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

Item {
    id: root

    property real radius: 0

    Rectangle {
        id: outsideBorder
        anchors {
            fill: parent
            margins: -border.width
        }

        color: "transparent"
        border {
            color: "yellow"
            width: 1
        }
        radius: root.radius + border.width
    }

    Rectangle {
        id: insideBorder
        anchors.fill: parent
        color: "transparent"
        border {
            color: "green"
            width: 1
        }
        radius: root.radius
    }
}

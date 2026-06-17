// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland
import org.deepin.dtk 1.0 as D

Blur {
    id: root
    property color color: Qt.rgba(1, 1, 1, 0.1)
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.color
    }
}

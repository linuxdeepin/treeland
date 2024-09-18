// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Waylib.Server
import org.deepin.dtk 1.0 as D

Item {
    id: root
    property int radius
    anchors.fill: parent
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "white"
        opacity: 0.1
    }
    RenderBufferBlitter {
        id: blitter
        z: root.parent.z - 1
        anchors.fill: parent
        MultiEffect {
            id: blur
            anchors.fill: parent
            source: blitter.content
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: 64
            saturation: 0.2
        }
    }
}

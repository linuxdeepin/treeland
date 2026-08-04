// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland

Rectangle {
    id: root

    color: Qt.rgba(0.0, 0.0, 0.0, 0.0)

    property bool radiusEnabled: radius > 0
    property int blurMax: Helper.config.blurStrength
    property bool blurEnabled: blurMax > 0 && blurAmount > 0
    property real blurAmount: Helper.config.blurAmount
    property real multiplier: Helper.config.blurMultiplier
    property real brightness: 0.0
    property real contrast: 0.0
    property real saturation: Helper.config.glassSaturation
    property real glassSpecular: Helper.config.glassSpecular
    property real glassTint: 0.0
    property bool glassEnabled: Helper.config.glassEnabled
    property Item effectContent: effectContentItem
    property bool contentOffscreen: false
    property bool effectEnabled: true
    readonly property Item content: contentContainer
    property alias offscreen: root.contentOffscreen
    default property alias data: contentContainer.data

    Item {
        id: contentContainer
        anchors.fill: parent
    }

    Rectangle {
        id: effectContentItem
        anchors.fill: parent
        radius: root.radius
        color: Qt.rgba(1.0, 1.0, 1.0, 0.15)
        visible: root.effectEnabled && !root.contentOffscreen
        z: -1
    }

    z: parent.z ? parent.z - 1 : -1
    anchors.fill: parent
}

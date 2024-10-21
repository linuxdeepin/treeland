// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Waylib.Server
import Treeland

RenderBufferBlitter {
    property real radius: 0
    property bool radiusEnabled: radius > 0
    property alias blurMax: blur.blurMax
    property alias blurEnabled: blur.blurEnabled
    property alias multiplier: blur.blurMultiplier

    id: blitter
    z: parent.z ? parent.z - 1 : -1
    anchors.fill: parent
    MultiEffect {
        id: blur
        anchors.fill: parent
        layer.enabled: blitter.radiusEnabled
        opacity: blitter.radiusEnabled ? 0 : parent.opacity
        source: blitter.content
        autoPaddingEnabled: false
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        saturation: 0.2
    }

    Loader {
        anchors.fill: parent
        active: blitter.radiusEnabled
        sourceComponent: TRadiusEffect {
            anchors.fill: parent
            sourceItem: blur
            radius: blitter.radius
        }
    }
}

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Treeland
import Waylib.Server
import org.deepin.dtk 1.0 as D

RenderBufferBlitter {
    required property SurfaceWrapper surface
    readonly property bool noRadius: surface.radius === 0 || surface.noCornerRadius

    id: blitter
    z: surface.z - 1
    anchors.fill: parent
    parent: surface
    MultiEffect {
        id: blur
        anchors.fill: parent
        layer.enabled: !blitter.noRadius
        opacity: !blitter.noRadius ? 0 : parent.opacity
        source: blitter.content
        autoPaddingEnabled: false
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        saturation: 0.2
    }

    Loader {
        anchors.fill: parent
        active: !blitter.noRadius
        sourceComponent: TRadiusEffect {
            anchors.fill: parent
            sourceItem: blur
            radius: surface.radius
        }
    }
}

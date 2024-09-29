// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects

Item {
    id: root

    property alias color: shadowSource.color
    property alias shadowEnabled: shadow.shadowEnabled
    property alias radius: shadowSource.radius
    readonly property rect boundingRect: Qt.rect(-shadow.blurMax, -shadow.blurMax,
                                                 width + 2 * shadow.blurMax,
                                                 height + 2 * shadow.blurMax)

    Rectangle {
        id: shadowSource
        anchors.fill: parent
        color: shadow.shadowColor
        layer {
            enabled: true
            live: true
        }
        visible: false
    }

    MultiEffect {
        id: shadow
        x: blurMax
        y: blurMax
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        shadowColor: shadowSource.color
        shadowBlur: 1.0
        blurMax: 64
        shadowEnabled: true
        shadowVerticalOffset: 10
        autoPaddingEnabled: true
        maskEnabled: true
        maskSource: shadowSource
        maskInverted: true
        source: shadowSource
    }
}

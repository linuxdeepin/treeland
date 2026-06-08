// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects

Item {
    id: root

    property alias sourceItem: effectSource.sourceItem
    property real radius: 10
    property rect targetRect: Qt.rect(0, 0, width, height)

    ShaderEffectSource {
        id: effectSource
        hideSource: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: effectSource
        maskEnabled: true
        maskSource: ShaderEffectSource {
            hideSource: true
            mipmap: true
            smooth: true
            samples: 8
            sourceItem: Item {
                width: root.width
                height: root.height
                Rectangle {
                    radius: root.radius
                    antialiasing: true
                    x: targetRect.x
                    y: targetRect.y
                    width: targetRect.width
                    height: targetRect.height
                }
            }
        }
    }
}

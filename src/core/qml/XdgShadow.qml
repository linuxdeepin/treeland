// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import org.deepin.dtk 1.0 as D
import Waylib.Server
import Treeland

D.BoxShadow {
    id: shadow

    required property SurfaceWrapper surface

    readonly property rect boundingRect: Qt.rect(-shadow.shadowBlur, -shadow.shadowBlur,
                                             width + 2 * shadow.shadowBlur,
                                             height + 2 * shadow.shadowBlur)

    visible: surface.shadow.radius > 0
    width: parent.width
    height: parent.height
    shadowColor: surface.shadow.color
    shadowOffsetX: surface.shadow.offsetX
    shadowOffsetY: surface.shadow.offsetY
    shadowBlur: surface.shadow.radius
    hollow: true
}

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Treeland
import org.deepin.dtk 1.0 as D

D.BoxShadow {
    required property SurfaceWrapper surface

    width: surface.width
    height: surface.height
    anchors.fill: surface.surfaceItem
    shadowColor: Qt.rgba(0, 0, 0, 0.4)
    shadowOffsetY: 10
    shadowBlur: 40
    cornerRadius: surface.radius
    hollow: true
}

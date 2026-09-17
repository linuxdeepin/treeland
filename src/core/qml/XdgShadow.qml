// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import org.deepin.dtk 1.0 as D

D.BoxShadow {
    id: shadow

    // boundingRect must cover the blur plus any signed offset. Because this
    // item is centered on the surface (anchors.centerIn in Decoration.qml), the
    // extents grow symmetrically by |offset| so the shadow stays aligned with
    // the surface while never clipping the offset shadow content.
    readonly property rect boundingRect: Qt.rect(
        -shadow.shadowBlur - Math.abs(shadow.shadowOffsetX),
        -shadow.shadowBlur - Math.abs(shadow.shadowOffsetY),
        width + 2 * shadow.shadowBlur + 2 * Math.abs(shadow.shadowOffsetX),
        height + 2 * shadow.shadowBlur + 2 * Math.abs(shadow.shadowOffsetY))

    width: parent.width
    height: parent.height
    shadowColor: Qt.rgba(0, 0, 0, 0.4)
    shadowOffsetX: 0
    shadowOffsetY: 10
    shadowBlur: 40
    hollow: true
}

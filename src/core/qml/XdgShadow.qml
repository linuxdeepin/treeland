// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import org.deepin.dtk 1.0 as D

D.BoxShadow {
    id: shadow
    readonly property rect boundingRect: Qt.rect(-shadow.shadowBlur, -shadow.shadowBlur,
                                             width + 2 * shadow.shadowBlur,
                                             height + 2 * shadow.shadowBlur)

    width: parent.width
    height: parent.height
    shadowColor: Qt.rgba(0, 0, 0, 0.4)
    shadowOffsetY: 10
    shadowBlur: 40
    hollow: true
}

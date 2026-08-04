// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland

Item {
    id: control

    property alias offscreen: blur.contentOffscreen
    property alias radius: blur.blurMax
    property alias multiplier: blur.multiplier
    property alias content: blur.effectContent
    default property alias data: blur.data
    property alias valid: blur.effectEnabled

    Blur {
        id: blur
        anchors.fill: parent
        glassEnabled: false
    }
}

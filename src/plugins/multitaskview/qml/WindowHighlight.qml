// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick

Rectangle {
    id: root

    required property real frameRadius
    required property bool padded
    required property real paddingRadius

    // The proxy already rounds CSD windows itself, so only the padding card can win here.
    readonly property real contourRadius: root.padded ? root.paddingRadius : root.frameRadius

    color: "transparent"
    border.color: "blue"
    radius: root.contourRadius > 0 ? root.contourRadius + root.border.width : 0
}

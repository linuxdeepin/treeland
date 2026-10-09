// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import Tinywl

Item {
    id: root

    required property SurfaceItem surface
    // surface?.parent maybe is a `SubsurfaceContainer`
    readonly property SurfaceWrapper wrapper: surface?.parent as SurfaceWrapper
    readonly property real cornerRadius: wrapper?.radius ?? 0

    anchors.fill: parent

    SurfaceItemContent {
        id: content
        surface: root.surface?.surface ?? null
        anchors.fill: parent
        opacity: alphaModifier
        live: root.surface && !(root.surface.flags & SurfaceItem.NonLive)
        smooth: root.surface?.smooth ?? true
        radius: effectEnabled ? cornerRadius : 0
        topLeftRadius: effectEnabled && root.surface.topPadding <= 0 ? cornerRadius : 0
        topRightRadius: effectEnabled && root.surface.topPadding <= 0 ? cornerRadius : 0
        bottomLeftRadius: effectEnabled ? cornerRadius : 0
        bottomRightRadius: effectEnabled ? cornerRadius : 0
        viewport: Qt.rect(-root.surface.leftPadding, -root.surface.topPadding,
                          root.surface.width * root.surface.surfaceSizeRatio,
                          root.surface.height * root.surface.surfaceSizeRatio)

        readonly property bool effectEnabled: !!root.wrapper
            && cornerRadius > 0
            && !root.wrapper.noCornerRadius

        onDevicePixelRatioChanged: {
            wrapper.updateSurfaceSizeRatio()
        }
    }
}

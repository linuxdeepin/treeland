// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import Treeland

Item {
    id: root

    required property SurfaceItem surface
    // surface?.parent maybe is a `SubsurfaceContainer`
    readonly property SurfaceWrapper wrapper: surface?.parent as SurfaceWrapper
    readonly property real cornerRadius: wrapper?.radius ?? 0

    anchors.fill: parent
    opacity: content.alphaModifier

    Loader {
        anchors.fill: parent
        active: wrapper?.blur ?? false
        sourceComponent: Blur {
            anchors.fill: parent
            radiusEnabled: cornerRadius > 0
            radius: cornerRadius
        }
    }

    SurfaceItemContent {
        id: content

        readonly property bool effectEnabled: GraphicsInfo.api !== GraphicsInfo.Software
            && !!root.wrapper
            && cornerRadius > 0
            && !root.wrapper.noCornerRadius
            && !!root.wrapper.decoration
            && root.wrapper.visibleDecoration

        surface: root.surface?.surface ?? null
        anchors.fill: parent
        live: root.surface && !(root.surface.flags & SurfaceItem.NonLive)
        smooth: root.surface?.smooth ?? true
        radius: effectEnabled ? cornerRadius : 0
        topLeftRadius: effectEnabled && wrapper?.noTitleBar ? cornerRadius : 0
        topRightRadius: effectEnabled && wrapper?.noTitleBar ? cornerRadius : 0
        bottomLeftRadius: effectEnabled ? cornerRadius : 0
        bottomRightRadius: effectEnabled ? cornerRadius : 0

        onDevicePixelRatioChanged: {
            if (wrapper) {
                wrapper.updateSurfaceSizeRatio()
            }
        }
    }

}

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
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
        opacity: effectLoader.active ? 0 : alphaModifier
        live: root.surface && !(root.surface.flags & SurfaceItem.NonLive)
        smooth: root.surface?.smooth ?? true

        onDevicePixelRatioChanged: {
            wrapper.updateSurfaceSizeRatio()
        }
    }

    Loader {
        id: effectLoader

        anchors.fill: parent
        active: {
            if (!root.wrapper)
                return false;
            return cornerRadius > 0 && !root.wrapper.noCornerRadius;
        }

        sourceComponent: RoundedClipEffect {
            sourceItem: content
            radius: cornerRadius
            opacity: content.alphaModifier
            targetRect: Qt.rect(-surface?.leftPadding ?? 0, -surface?.topPadding ?? 0,
                                root.surface?.width * root.surface?.surfaceSizeRatio ?? 0,
                                root.surface?.height * root.surface?.surfaceSizeRatio ?? 0)
        }
    }
}

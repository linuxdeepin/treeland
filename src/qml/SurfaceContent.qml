// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import Treeland

Item {
    id: root

    required property SurfaceItem surface
    readonly property SurfaceWrapper wrapper: surface?.parent ?? null
    readonly property real cornerRadius: wrapper?.radius ?? cornerRadius

    anchors.fill: parent
    SurfaceItemContent {
        id: content
        surface: root.surface?.surface ?? null
        anchors.fill: parent
        opacity: effectLoader.active ? 0 : 1
        live: root.surface && !(root.surface.flags & SurfaceItem.NonLive)
        smooth: root.surface?.smooth ?? true
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
            targetRect: Qt.rect(-surface?.leftPadding ?? 0, -surface?.topPadding ?? 0,
                                root.surface?.width ?? 0, root.surface?.height ?? 0)
        }
    }
}

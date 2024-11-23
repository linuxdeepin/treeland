// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Shapes
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
        opacity: effectLoader.active ? 0 : parent.opacity
        live: root.surface && !(root.surface.flags & SurfaceItem.NonLive)
        smooth: root.surface?.smooth ?? true
    }

    Loader {
        id: effectLoader

        anchors.fill: parent
        active: {
            if (GraphicsInfo.api === GraphicsInfo.Software)
                return false;

            if (!root.wrapper)
                return false;
            return cornerRadius > 0 && !root.wrapper.noCornerRadius;
        }

        sourceComponent: Shape {
            x: content.x
            y: content.y
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: 0
                fillItem: content
                PathRectangle {
                    width: content.width
                    height: content.height
                    bottomLeftRadius: cornerRadius
                    bottomRightRadius: cornerRadius
                }
            }
        }
    }
}

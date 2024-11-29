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
            fillMode: Shape.PreserveAspectFit
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: 0
                fillItem: content
                PathRectangle {
                    readonly property real scale: width / content.width

                    x: content.bufferSourceRect.x
                    y: content.bufferSourceRect.y
                    width: content.bufferSourceRect.width
                    height: content.bufferSourceRect.height
                    topLeftRadius: wrapper?.noTitleBar ? cornerRadius * scale : 0
                    topRightRadius: wrapper?.noTitleBar ? cornerRadius * scale : 0
                    bottomLeftRadius: cornerRadius * scale
                    bottomRightRadius: cornerRadius * scale
                }
            }
        }
    }
}

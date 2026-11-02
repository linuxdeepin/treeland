// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Shapes
import Waylib.Server
import Treeland

Item {
    id: root

    required property SurfaceItem surface
    // surface?.parent maybe is a `SubsurfaceContainer`
    readonly property SurfaceWrapper wrapper: surface?.parent as SurfaceWrapper
    readonly property XWaylandSurface xwaylandSurface: wrapper?.shellSurface as XWaylandSurface
    readonly property bool hasClipShape: xwaylandSurface?.hasClipShape ?? false
    readonly property real cornerRadius: wrapper?.radius ?? 0

    anchors.fill: parent
    opacity: content.alphaModifier

    function clipShapePaths() {
        if (!hasClipShape)
            return [];

        const geometry = xwaylandSurface.geometry;
        if (geometry.width <= 0 || geometry.height <= 0)
            return [];

        const scaleX = width / geometry.width;
        const scaleY = height / geometry.height;
        const paths = [];
        for (const contour of xwaylandSurface.clipShape) {
            const points = [];
            for (const point of contour)
                points.push(Qt.point(point.x * scaleX, point.y * scaleY));
            paths.push(points);
        }
        return paths;
    }

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
        surface: root.surface?.surface ?? null
        anchors.fill: parent
        opacity: (effectLoader.active || shapeLoader.active) ? 0 : parent.opacity
        live: root.surface && (root.wrapper?.renderingEnabled ?? true)
              && !(root.surface.flags & SurfaceItem.NonLive)
        smooth: root.surface?.smooth ?? true

        onDevicePixelRatioChanged: {
            // The wrapper outlives a destroyed shell surface during its close
            // animation, so there is nothing to keep in sync in that case.
            if (wrapper && wrapper.shellSurface) {
                wrapper.updateSurfaceSizeRatio()
            }
        }
    }

    Loader {
        id: effectLoader

        anchors.fill: parent
        active: {
            if (GraphicsInfo.api === GraphicsInfo.Software)
                return false;

            if (!root.wrapper)
                return false;
            return (cornerRadius > 0) &&
                    !root.wrapper.noCornerRadius &&
                    root.wrapper.decoration &&
                    root.wrapper.visibleDecoration;
        }

        opacity: shapeLoader.active ? 0 : 1
        layer.enabled: shapeLoader.active

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

    Loader {
        id: shapeLoader

        anchors.fill: parent
        active: root.hasClipShape && GraphicsInfo.api !== GraphicsInfo.Software
        sourceComponent: Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: 0
                fillItem: effectLoader.active ? effectLoader : content
                fillTransform: effectLoader.active
                    ? PlanarTransform.fromScale(1, 1, 0, 0)
                    : PlanarTransform.fromScale(
                          content.bufferSourceRect.width > 0
                              ? root.width / content.bufferSourceRect.width : 1,
                          content.bufferSourceRect.height > 0
                              ? root.height / content.bufferSourceRect.height : 1,
                          0, 0)
                fillRule: ShapePath.OddEvenFill
                PathMultiline {
                    paths: root.clipShapePaths()
                }
            }
        }
    }
}

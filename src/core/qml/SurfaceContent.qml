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
    readonly property real cornerRadius: wrapper?.frameGeometry.radius ?? 0
    readonly property real frameScale: 1 / (surface?.contentItem?.scale ?? 1)
    readonly property point frameOffset: Qt.point(
        -(surface?.contentItem?.x ?? 0) * frameScale,
        -(surface?.contentItem?.y ?? 0) * frameScale)
    readonly property rect effectFrameRect: wrapper
        ? Qt.rect(frameOffset.x, frameOffset.y,
                  wrapper.frameGeometry.rect.width * frameScale,
                  wrapper.frameGeometry.rect.height * frameScale)
        : Qt.rect(0, 0, width, height)

    Loader {
        id: clientMask
        active: root.cornerRadius > 0
        sourceComponent: FrameInputMask {
            geometry: root.wrapper.frameGeometry
            frameItem: root.wrapper.frameItem
            target: root.surface?.eventItem ?? null
        }
    }
    Binding {
        target: root.surface?.eventItem ?? null
        property: "containmentMask"
        value: clientMask.item
        when: clientMask.active && target !== null
    }

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
            radius: root.cornerRadius * root.frameScale
            frameRect: root.effectFrameRect
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
        active: root.cornerRadius > 0

        opacity: shapeLoader.active ? 0 : 1
        layer.enabled: shapeLoader.active

        sourceComponent: FrameClip {
            source: content
            sourceRect: content.bufferSourceRect
            geometry: root.wrapper.frameGeometry
            geometryScale: root.frameScale
            geometryOffset: root.frameOffset
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

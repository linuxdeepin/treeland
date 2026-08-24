// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

OutputItem {
    id: outputItem

    required property SourceOutput targetOutputItem
    property OutputViewport screenViewport: viewport

    property OutputViewport sourceScreenViewport: targetOutputItem.screenViewport

    // Constants for fallback and precision thresholds
    readonly property int fallbackSize: 100
    readonly property real sizeMatchTolerance: 0.1
    readonly property real scaleEpsilon: 0.001

    // Helper property: source screen pixel size (physical size in pixels)
    // Formula: pixelSize = effectiveSize * scale
    // - output.size returns effectiveSize (logical size)
    // - scale is the devicePixelRatio
    // - pixelSize is the actual framebuffer size in pixels
    // Fallback is used during component initialization or if source output is not ready
    readonly property size sourcePixelSize: {
        if (!sourceScreenViewport || !sourceScreenViewport.output) {
            console.warn("MirrorOutput: Source viewport not ready, using fallback size");
            return Qt.size(fallbackSize, fallbackSize);
        }
        const effectiveSize = sourceScreenViewport.output.size;
        const scale = sourceScreenViewport.output.scale;
        const rotatedSize = Qt.size(effectiveSize.width * scale, effectiveSize.height * scale);

        const sourceRotation = sourceScreenViewport.rotation;
        const isSourceRotated90or270 = (Math.abs(sourceRotation % 180) === 90);

        if (isSourceRotated90or270) {
            return Qt.size(rotatedSize.height, rotatedSize.width);
        }
        return rotatedSize;
    }

    devicePixelRatio: output?.scale ?? devicePixelRatio

    Rectangle {
        id: content
        anchors.fill: parent
        color: "black"

        TextureProxy {
            id: proxy
            sourceItem: sourceScreenViewport
            anchors.centerIn: parent
            rotation: targetOutputItem.keepAllOutputRotation ? 0 : sourceScreenViewport.rotation

            width: sourcePixelSize.width
            height: sourcePixelSize.height
            sourceRect: Qt.rect(0, 0, sourcePixelSize.width, sourcePixelSize.height)

            smooth: true
            transformOrigin: Item.Center

            scale: {
                if (!sourceScreenViewport || !sourceScreenViewport.output) {
                    return 1.0;
                }
                if (!content.width || !content.height) {
                    return 1.0;
                }
                // Wait for TextureProxy size to sync with source pixel size
                if (Math.abs(width - sourcePixelSize.width) > sizeMatchTolerance ||
                    Math.abs(height - sourcePixelSize.height) > sizeMatchTolerance) {
                    return 1.0;
                }

                const proxyRotation = rotation;
                const isRotated90or270 = (Math.abs(proxyRotation % 180) === 90);
                const visualWidth = isRotated90or270 ? height : width;
                const visualHeight = isRotated90or270 ? width : height;

                if (visualWidth <= 0 || visualHeight <= 0) {
                    return 1.0;
                }

                const scaleX = content.width / visualWidth;
                const scaleY = content.height / visualHeight;
                const finalScale = Math.min(scaleX, scaleY);

                // Fix floating-point precision issues
                return Math.abs(finalScale - 1.0) < scaleEpsilon ? 1.0 : finalScale;
            }
        }
    }

    OutputViewport {
        id: viewport

        anchors.centerIn: parent
        depends: [sourceScreenViewport]
        devicePixelRatio: outputItem.devicePixelRatio
        input: content
        output: outputItem.output

        RotationAnimation {
            id: rotationAnimator
            target: viewport
            duration: 200
            alwaysRunToEnd: true
        }

        Timer {
            id: transformTimer
            property var scheduleTransform
            onTriggered: viewport.rotateOutput(scheduleTransform)
            interval: rotationAnimator.duration / 2
        }

        function rotationOutput(orientation) {
            transformTimer.scheduleTransform = orientation
            transformTimer.start()

            switch(orientation) {
            case WaylandOutput.R90:
                rotationAnimator.to = 90
                break
            case WaylandOutput.R180:
                rotationAnimator.to = 180
                break
            case WaylandOutput.R270:
                rotationAnimator.to = -90
                break
            default:
                rotationAnimator.to = 0
                break
            }

            rotationAnimator.from = rotation
            rotationAnimator.start()
        }
    }

    function setTransform(transform) {
        viewport.rotationOutput(transform)
    }

    function setScale(scale) {
        viewport.setOutputScale(scale)
    }

    function invalidate() {
        viewport.invalidate()
    }
}

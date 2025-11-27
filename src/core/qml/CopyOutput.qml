// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

OutputItem {
    id: outputItem

    required property PrimaryOutput targetOutputItem
    property OutputViewport screenViewport: viewport

    property OutputViewport primaryScreenViewport: targetOutputItem.screenViewport

    // Constants for fallback and precision thresholds
    readonly property int fallbackSize: 100
    readonly property real sizeMatchTolerance: 0.1
    readonly property real scaleEpsilon: 0.001

    // Helper property: primary screen pixel size (physical size in pixels)
    // Formula: pixelSize = effectiveSize * scale
    // - output.size returns effectiveSize (logical size)
    // - scale is the devicePixelRatio
    // - pixelSize is the actual framebuffer size in pixels
    // Fallback is used during component initialization or if primary output is not ready
    readonly property size primaryPixelSize: {
        if (!primaryScreenViewport || !primaryScreenViewport.output) {
            console.warn("CopyOutput: Primary viewport not ready, using fallback size");
            return Qt.size(fallbackSize, fallbackSize);
        }
        const effectiveSize = primaryScreenViewport.output.size;
        const scale = primaryScreenViewport.output.scale;
        const rotatedSize = Qt.size(effectiveSize.width * scale, effectiveSize.height * scale);

        const primaryRotation = primaryScreenViewport.rotation;
        const isPrimaryRotated90or270 = (Math.abs(primaryRotation % 180) === 90);

        if (isPrimaryRotated90or270) {
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
            sourceItem: primaryScreenViewport
            anchors.centerIn: parent
            rotation: targetOutputItem.keepAllOutputRotation ? 0 : primaryScreenViewport.rotation

            width: primaryPixelSize.width
            height: primaryPixelSize.height
            sourceRect: Qt.rect(0, 0, primaryPixelSize.width, primaryPixelSize.height)

            smooth: true
            transformOrigin: Item.Center

            scale: {
                if (!primaryScreenViewport || !primaryScreenViewport.output) {
                    return 1.0;
                }
                if (!content.width || !content.height) {
                    return 1.0;
                }

                // Wait for TextureProxy size to sync with primary pixel size
                if (Math.abs(width - primaryPixelSize.width) > sizeMatchTolerance ||
                    Math.abs(height - primaryPixelSize.height) > sizeMatchTolerance) {
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
        depends: [primaryScreenViewport]
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

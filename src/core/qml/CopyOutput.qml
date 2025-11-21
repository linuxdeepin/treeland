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
        // Calculate pixel size: effectiveSize * scale
        return Qt.size(effectiveSize.width * scale, effectiveSize.height * scale);
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

                // Calculate scale: content size / primary screen pixel size
                const scaleX = content.width / primaryPixelSize.width;
                const scaleY = content.height / primaryPixelSize.height;
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
        ignoreViewport: true
    }
}

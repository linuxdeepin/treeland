// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

// The tooltip presentation of the popover: the surface on screen and the layers
// it left behind.
QtObject {
    id: root

    // The DockPreview controller: logging category.
    required property QtObject preview
    // Item that owns the tooltip layers.
    property Item host: null
    // Creates one tooltip layer.
    property Component layerComponent: null

    property var liveLayer: null
    // Natural size of the tooltip that last left.
    property size lastSize: Qt.size(0, 0)

    readonly property size liveSize: liveLayer ? liveLayer.naturalSize : Qt.size(0, 0)
    // Natural size of the tooltip on screen, falling back to the one that just
    // left: a switch never has to measure a layer that is already fading out.
    readonly property size naturalSize: liveSize.width > 0 ? liveSize : lastSize

    // `surface` is the reported tooltip surface; null means it went away.
    function applySurface(surface) {
        const live = liveLayer;
        if (surface)
            createLive(surface);
        demote(live);
    }

    function createLive(surface) {
        if (!layerComponent)
            return;

        const layer = layerComponent.createObject(host);
        if (!layer) {
            console.warn(preview.loggingCategory, "failed to create the tooltip layer");
            return;
        }
        if (!layer.setSurface(surface)) {
            layer.destroy();
            return;
        }
        liveLayer = layer;
        layer.fadeIn();
    }

    // Every tooltip the dock stops showing becomes a frozen layer.
    function demote(layer) {
        if (!layer)
            return;

        // Snapshot the size before clearing the live layer: naturalSize falls
        // back to lastSize whenever there is no live one, so this keeps the
        // frame target from dipping during a switch.
        const size = layer.naturalSize;
        if (size.width > 0 && size.height > 0)
            lastSize = size;
        if (liveLayer === layer)
            liveLayer = null;
        layer.freeze();
        layer.fadeOut();
    }
}

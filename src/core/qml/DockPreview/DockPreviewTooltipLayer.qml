// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

import Waylib.Server
import Treeland.Protocols

// One tooltip surface drawn in the popover. A layer either shows a live surface
// or keeps the last frame of one that already left.
Item {
    id: root

    // The DockPreview controller: frame geometry, direction and durations.
    required property QtObject preview

    // The frame this layer shows.
    property Item surfaceItem: null
    property bool fadingOut: false

    readonly property size naturalSize: surfaceItem
                                         ? Qt.size(surfaceItem.implicitWidth,
                                                   surfaceItem.implicitHeight)
                                         : Qt.size(0, 0)

    // Where the layer sits inside the frame, for the current direction.
    // Position and the scale origin both read the same animated frame on the
    // controller, so they cannot disagree.
    readonly property real layerX: {
        switch (preview.direction) {
        case ForeignToplevelManagerInterfaceV3.PreviewDirection.left:
            return 0;
        case ForeignToplevelManagerInterfaceV3.PreviewDirection.right:
            return preview.frameWidth - width;
        default: // top / bottom: centred on the dock icon
            return (preview.frameWidth - width) / 2;
        }
    }

    readonly property real layerY: {
        switch (preview.direction) {
        case ForeignToplevelManagerInterfaceV3.PreviewDirection.top:
            return 0;
        case ForeignToplevelManagerInterfaceV3.PreviewDirection.bottom:
            return preview.frameHeight - height;
        default: // left / right: centred on the dock icon
            return (preview.frameHeight - height) / 2;
        }
    }

    // Every layer shrinks towards the edge the tooltip is glued to.
    readonly property point scaleOrigin: preview.scaleOriginFor(width, height)
    // Fit into the frame without ever stretching it.
    readonly property real compressionX:
        width > 0 && preview.frameWidth > 0 ? Math.min(1, preview.frameWidth / width) : 1
    readonly property real compressionY:
        height > 0 && preview.frameHeight > 0 ? Math.min(1, preview.frameHeight / height) : 1

    z: 1
    visible: surfaceItem !== null
    width: naturalSize.width
    height: naturalSize.height

    x: root.layerX
    y: root.layerY

    transform: Scale {
        origin.x: root.scaleOrigin.x
        origin.y: root.scaleOrigin.y
        xScale: root.compressionX
        yScale: root.compressionY
    }

    Component {
        id: surfaceComponent

        SurfaceItem {
            flags: SurfaceItem.RejectEvent
            // Its natural size, never stretched; the owning layer is what gets
            // positioned and compressed.
            width: implicitWidth
            height: implicitHeight
        }
    }

    // Build the surface item for `surface`, unless it cannot be created.
    function setSurface(surface) {
        const item = surfaceComponent.createObject(root);
        if (!item) {
            console.warn(preview.loggingCategory, "failed to create the tooltip surface item");
            return false;
        }
        // Set the surface on an already completed item, so it is initialized the
        // same way the previously static item was, and has its size right away.
        item.surface = surface;
        surfaceItem = item;
        return true;
    }

    NumberAnimation {
        id: fadeAnimation
        target: root
        property: "opacity"
        duration: preview.aniDuration
        easing.type: Easing.OutExpo
        onFinished: {
            if (root.fadingOut)
                root.destroy();
        }
    }

    function freeze() {
        if (!surfaceItem)
            return;
        surfaceItem.surface = null;
        surfaceItem.enabled = false;
    }

    function fadeIn() {
        fadingOut = false;
        if (!preview.enableAnimation) {
            opacity = 1;
            return;
        }
        opacity = 0;
        fadeAnimation.from = 0;
        fadeAnimation.to = 1;
        fadeAnimation.restart();
    }

    // A demoted layer only owns its fade from here on.
    function fadeOut() {
        fadingOut = true;
        if (!preview.enableAnimation) {
            root.destroy();
            return;
        }
        fadeAnimation.from = opacity;
        fadeAnimation.to = 0;
        fadeAnimation.restart();
    }
}

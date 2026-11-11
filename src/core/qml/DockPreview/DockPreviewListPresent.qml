// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

Item {
    id: root

    objectName: "listPresent"
    required property QtObject preview

    property var liveLayer: null
    property var fadingLayers: []

    // A signal instead of a close() call: whether an empty list closes the
    // popover or only clears the list is the controller's decision.
    signal emptied()

    x: preview.contentLeft
    y: preview.contentTop
    width: preview.contentWidth
    height: preview.contentHeight
    // The shell fade covers enter and leave; the reveal adds the hand-over.
    opacity: preview.shellFade * preview.listReveal
    visible: root.liveLayer !== null || root.fadingLayers.length > 0
    clip: true

    function createLayer() {
        return layerComponent.createObject(root);
    }

    function dropLayer(layer) {
        if (!layer)
            return;

        const index = root.fadingLayers.indexOf(layer);
        if (index >= 0) {
            const fading = root.fadingLayers.slice();
            fading.splice(index, 1);
            root.fadingLayers = fading;
        }
        if (root.liveLayer === layer)
            root.liveLayer = null;
        layer.destroy();
    }

    // A dock icon keeps its windows when its list is refreshed, and two icons
    // never share one. A list with nothing in common with the live one is a
    // different icon, not a refreshed list.
    function sharesWindow(layer, wrappers) {
        for (let i = 0; i < layer.rows.length; ++i) {
            const row = layer.rows[i];
            for (let k = 0; k < wrappers.length; ++k) {
                if (row.wrapper === wrappers[k])
                    return true;
            }
        }
        return false;
    }

    function sync() {
        const wrappers = [ ];
        const surfaces = preview.desiredSurfaces;
        for (let i = 0; i < surfaces.length; ++i) {
            if (surfaces[i])
                wrappers.push(surfaces[i]);
        }

        if (wrappers.length === 0) {
            root.emptied();
            return;
        }

        const handover = root.liveLayer
                         && !root.sharesWindow(root.liveLayer, wrappers);
        if (!root.liveLayer || handover) {
            const layer = root.createLayer();
            if (!layer) {
                console.warn(preview.loggingCategory, "failed to create a preview list");
                return;
            }
            if (root.liveLayer)
                root.demote(root.liveLayer);

            layer.sync(wrappers, true);
            root.liveLayer = layer;
            if (handover)
                layer.fadeIn();
            return;
        }

        root.liveLayer.sync(wrappers, false);
    }

    function demote(layer) {
        if (!layer)
            return;

        const fading = root.fadingLayers.slice();
        fading.push(layer);
        root.fadingLayers = fading;
        layer.fadeOut();
    }

    function freezeForClose() {
        if (root.liveLayer)
            root.liveLayer.freezePositions();
    }

    function clear() {
        const layers = root.fadingLayers.slice();
        if (root.liveLayer)
            layers.unshift(root.liveLayer);
        root.liveLayer = null;
        root.fadingLayers = [ ];
        for (let i = 0; i < layers.length; ++i)
            layers[i].destroy();
    }

    Component {
        id: layerComponent

        DockPreviewListLayer {
            preview: root.preview
            listPresent: root
        }
    }
}

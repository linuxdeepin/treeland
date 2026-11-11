// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

import Treeland

// One list instance: the rows for one dock icon. A switch to another icon
// starts a new instance and fades this one out, so the rows never change what
// they show under the pointer.
Item {
    id: listLayer

    required property QtObject preview
    required property QtObject listPresent

    property var rows: []
    property var fadingRows: []
    property real rowsHeight: 0
    property bool fadingOut: false

    // Input follows the live layer only: an outgoing list keeps drawing while
    // it fades, but must not preview or activate windows.
    readonly property bool interactive:
        preview.requestedPresentation === DockPreview.Presentation.List
        && listPresent.liveLayer === listLayer
        && !listLayer.fadingOut

    // The rows scroll past this height; the frame never grows for them.
    readonly property real viewportHeight:
        Math.min(preview.listMaxViewportHeight, listLayer.rowsHeight)
    // A settled list draws at its own size so the frame animates around it;
    // fitting it would shrink the list every time a row is added or removed.
    readonly property bool drawsAtOwnSize:
        preview.listReveal === 1
        && listPresent.liveLayer === listLayer
        && listPresent.fadingLayers.length === 0
    readonly property real fitScale: {
        if (listLayer.drawsAtOwnSize)
            return 1;
        if (!(width > 0) || !(height > 0))
            return 1;
        return Math.max(0, Math.min(1, preview.contentWidth / width,
                                     preview.contentHeight / height));
    }

    width: preview.listContentWidth
    height: listLayer.viewportHeight
    // Left aligned. A fitted list also keeps its bottom on the frame's content
    // bottom, so it grows out of the dock edge.
    y: (listLayer.drawsAtOwnSize || !preview.listPinnedToBottom)
       ? 0
       : Math.max(0, preview.contentHeight - listLayer.height * listLayer.fitScale)
    transform: Scale {
        origin.x: 0
        origin.y: 0
        xScale: listLayer.fitScale
        yScale: xScale
    }

    function createRow() {
        return rowComponent.createObject(rowLayer);
    }

    function dropRow(row) {
        if (!row)
            return;

        const index = listLayer.fadingRows.indexOf(row);
        if (index >= 0) {
            const fading = listLayer.fadingRows.slice();
            fading.splice(index, 1);
            listLayer.fadingRows = fading;
        }
        row.destroy();
    }

    // Height the live layout needs. Rows collapsing out do not count: the frame
    // is already shrinking around them.
    function refreshHeight() {
        let extent = 0;
        for (let i = 0; i < listLayer.rows.length; ++i) {
            const row = listLayer.rows[i];
            extent = Math.max(extent, row.yTo + row.height);
        }
        listLayer.rowsHeight = extent;
        // A shorter list must not leave the scroller past its end.
        const maxContentY = scroller.contentHeight - scroller.height;
        scroller.contentY = Math.max(0, Math.min(scroller.contentY,
                                                 maxContentY));
    }

    // Bring the rows in line with `wrappers`. A row is never reused for another
    // window: the title would change under the pointer.
    function sync(wrappers, instant) {
        const rects = preview.listRowRects(wrappers);
        const previous = listLayer.rows;
        const used = new Array(previous.length).fill(false);
        const live = [ ];

        for (let i = 0; i < wrappers.length; ++i) {
            let kept = null;
            for (let k = 0; k < previous.length; ++k) {
                if (!used[k] && previous[k].wrapper === wrappers[i]) {
                    used[k] = true;
                    kept = previous[k];
                    break;
                }
            }
            if (kept) {
                kept.retarget(wrappers[i], rects[i].y);
                live.push(kept);
                continue;
            }

            const created = listLayer.createRow();
            if (!created) {
                console.warn(preview.loggingCategory, "failed to create a preview row");
                continue;
            }
            const startTop = live.length ? live[live.length - 1].y
                                         : rects[i].y;
            created.enter(wrappers[i], startTop, rects[i].y, instant);
            live.push(created);
        }

        const fading = listLayer.fadingRows.slice();
        for (let k = 0; k < previous.length; ++k) {
            if (used[k])
                continue;

            // Close into the row that takes its place: the last row always has
            // one before it, so `rects` is never empty here.
            const row = previous[k];
            const target = rects[Math.min(row.rowIndex, rects.length - 1)].y;
            if (row.collapseTo(target))
                fading.push(row);
        }
        listLayer.fadingRows = fading;

        listLayer.rows = live;
        for (let i = 0; i < live.length; ++i)
            live[i].rowIndex = i;

        listLayer.refreshHeight();
    }

    // The shell is fading out with no windows left: stop the rows where they
    // are and let the fade take them.
    function freezePositions() {
        for (let i = 0; i < listLayer.rows.length; ++i)
            listLayer.rows[i].freezePosition();
        for (let i = 0; i < listLayer.fadingRows.length; ++i)
            listLayer.fadingRows[i].freezePosition();
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

    function fadeOut() {
        fadingOut = true;
        if (!preview.enableAnimation) {
            listPresent.dropLayer(listLayer);
            return;
        }
        fadeAnimation.from = opacity;
        fadeAnimation.to = 0;
        fadeAnimation.restart();
    }

    Flickable {
        id: scroller

        width: listLayer.width
        height: listLayer.viewportHeight
        contentWidth: width
        contentHeight: listLayer.rowsHeight
        enabled: listLayer.interactive
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
        // The frame is the only clip: a collapsing or scrolled-out row is cut
        // there, not by the list.
        clip: false

        Item {
            id: rowLayer
            width: scroller.width
            height: scroller.contentHeight
        }

        HoverHandler {
            enabled: listLayer.interactive
            cursorShape: Qt.ArrowCursor
            onHoveredChanged: {
                if (!hovered)
                    preview.stopPreviewWindow();
            }
        }
    }

    Component {
        id: rowComponent

        DockPreviewListRow {
            preview: listLayer.preview
            layerItem: listLayer
        }
    }

    NumberAnimation {
        id: fadeAnimation
        target: listLayer
        property: "opacity"
        duration: preview.aniDuration
        easing.type: Easing.OutExpo
        onFinished: {
            if (listLayer.fadingOut)
                listPresent.dropLayer(listLayer);
        }
    }
}

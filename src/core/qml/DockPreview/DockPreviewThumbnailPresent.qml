// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts

import Waylib.Server
import Treeland
import org.deepin.dtk 1.0 as D

Item {
    id: root

    required property QtObject preview
    // Whether the pointer is over the popover.
    property bool hovered: false

    // Corner radius shared by the slots and the hover frame.
    property int radius: PopoverStyle.itemRadius
    // Topmost z of the preview slots.
    readonly property int slotZBase: 10000
    // Top of the content the title row takes for itself, in step with its own
    // fade. The shared content rect never moves for it.
    readonly property real titleSpace:
        preview.thumbnailReveal * preview.titleRowChrome
    // Slots in view order: live ones first, then the collapsing ones.
    property var slots: []
    // List row the title follows.
    property int currentSlot: 0
    // The slot under the pointer; drives the frame's opacity.
    property var hoveredSlot: null
    // The slot the frame is drawn on. Kept after the pointer leaves so the
    // frame fades out in place; cleared once that slot is gone.
    property var highlightSlot: null
    onHoveredSlotChanged: {
        if (root.hoveredSlot)
            root.highlightSlot = root.hoveredSlot;
    }

    // Room the view keeps for the highlight frame; the layout ignores it.
    readonly property int frameMargin: PopoverStyle.frameBorderWidth

    // The window the title row describes: the highlighted preview, clamped.
    readonly property var currentWrapper: {
        const surfaces = preview.desiredSurfaces;
        if (surfaces.length === 0)
            return null;
        const index = Math.min(Math.max(root.currentSlot, 0), surfaces.length - 1);
        return surfaces[index] ?? null;
    }

    // A signal instead of a close() call: whether an empty list closes the
    // popover or only clears the thumbnails is the controller's decision.
    signal emptied()

    anchors.fill: parent
    visible: preview.thumbnailReveal > 0
    // The shell fade covers enter and leave; the slots add the presentation
    // reveal.
    opacity: preview.shellFade

    // Keep the last picture of every slot, for a close that fades out with
    // nothing taking the windows over. The windows must still be alive.
    function freezeForClose() {
        const slots = root.slots;
        for (let i = 0; i < slots.length; ++i)
            slots[i].freezePicture();
    }

    // Pick the slot under the pointer again after the one it was on went away.
    function refreshHoveredSlot() {
        const slots = root.slots;
        let hovered = null;
        for (let i = 0; i < slots.length; ++i) {
            if (!slots[i].collapsing && slots[i].hovered) {
                hovered = slots[i];
                break;
            }
        }
        root.hoveredSlot = hovered;
    }

    // Clears the frame's slot before it is destroyed.
    function forgetSlot(slot) {
        if (root.highlightSlot === slot)
            root.highlightSlot = null;
    }

    function dropSlot(slot) {
        if (!slot)
            return;

        const slots = root.slots;
        const index = slots.indexOf(slot);
        if (index >= 0) {
            const next = slots.slice();
            next.splice(index, 1);
            root.slots = next;
        }

        root.forgetSlot(slot);
        slot.destroy();
    }

    // Drop every slot and forget which one was highlighted.
    function clear() {
        const slots = root.slots;
        root.slots = [ ];
        root.hoveredSlot = null;
        root.highlightSlot = null;
        for (let i = 0; i < slots.length; ++i)
            slots[i].destroy();
    }

    function createSlot() {
        return slotComponent.createObject(slotLayer);
    }

    // Bring the slot instances in line with the controller's window list.
    function sync() {
        const wrappers = [ ];
        const surfaces = preview.desiredSurfaces;
        for (let i = 0; i < surfaces.length; ++i) {
            if (surfaces[i])
                wrappers.push(surfaces[i]);
        }

        if (wrappers.length === 0) {
            root.currentSlot = 0;
            root.emptied();
            return;
        }

        // A strip that keeps none of its slots is a new strip: it starts at the
        // dock-facing end, whatever asked for it.
        const fresh = !root.slots.some(slot => wrappers.includes(slot.wrapper));
        if (fresh) {
            root.currentSlot = 0;
            scrollHint.stopPage();
            slotView.contentX = 0;
            slotView.contentY = 0;
        }

        // Target rects: where every window should sit in the view.
        const rects = preview.slotRects(wrappers);
        // Old instances: the live slots, then the collapsing ones. A
        // collapsing slot can still take its window back until it is gone.
        const previous = root.slots;
        const pool = [ ];
        for (let i = 0; i < previous.length; ++i)
            pool.push({ slot: previous[i], retiring: previous[i].collapsing });

        const assigned = new Array(wrappers.length).fill(-1);
        const used = new Array(pool.length).fill(false);

        // Match: a window still in the list keeps its own slot.
        for (let i = 0; i < wrappers.length; ++i) {
            for (let k = 0; k < pool.length; ++k) {
                if (!used[k] && pool[k].slot.wrapper === wrappers[i]) {
                    assigned[i] = k;
                    used[k] = true;
                    break;
                }
            }
        }

        // The rest take the remaining slots in order: content switches.
        let next = 0;
        for (let i = 0; i < wrappers.length; ++i) {
            if (assigned[i] >= 0)
                continue;
            while (next < pool.length && used[next])
                ++next;
            if (next >= pool.length)
                break;
            assigned[i] = next;
            used[next] = true;
        }

        // Where every old slot ends up: the rect of the slot it closes into.
        // A slot at the front has no slot before it, so the one taking its
        // place is what it closes into.
        const closeRect = new Array(pool.length).fill(null);
        for (let i = 0; i < wrappers.length; ++i) {
            if (assigned[i] >= 0)
                closeRect[assigned[i]] = rects[i];
        }

        let firstKept = 0;
        while (firstKept < pool.length && closeRect[firstKept] === null)
            ++firstKept;

        for (let k = 0; k < pool.length; ++k) {
            if (closeRect[k] === null)
                closeRect[k] = closeRect[k < firstKept ? firstKept : k - 1];
        }

        const lastPooled = pool.length ? pool[pool.length - 1].slot : null;
        const entryRect = lastPooled ? lastPooled.currentRect() : null;
        const instant = !entryRect;

        // Build the new sequence.
        const live = [ ];
        const nextRetiring = [ ];

        for (let i = 0; i < wrappers.length; ++i) {
            const k = assigned[i];
            if (k >= 0) {
                const slot = pool[k].slot;
                if (pool[k].retiring) {
                    slot.revive(wrappers[i], rects[i]);
                    live.push(slot);
                } else {
                    slot.retarget(wrappers[i], rects[i]);
                    live.push(slot);
                }
                continue;
            }

            const created = root.createSlot();
            if (!created) {
                console.warn(preview.loggingCategory, "failed to create a preview slot");
                continue;
            }
            created.enter(wrappers[i], entryRect ? entryRect : rects[i], rects[i], instant);
            live.push(created);
        }

        // Unclaimed: every one of them collapses forward, into the slot before
        // it, and only leaves once that is done.
        for (let k = 0; k < pool.length; ++k) {
            if (used[k])
                continue;

            const slot = pool[k].slot;
            // Already on its way out; its own close is what fades it.
            if (pool[k].retiring) {
                nextRetiring.push(slot);
                continue;
            }
            if (slot.collapseTo(closeRect[k]))
                nextRetiring.push(slot);
        }

        // Bookkeeping: slot order, z, current slot, hover.
        root.slots = live.concat(nextRetiring);

        for (let i = 0; i < live.length; ++i) {
            live[i].slotIndex = i;
            // Later slots stay below, so a new one comes out from behind.
            live[i].z = root.slotZBase - i;
        }
        for (let i = 0; i < nextRetiring.length; ++i)
            nextRetiring[i].z = 0;

        // The title row follows its slot when the slots renumber.
        if (root.hoveredSlot && live.indexOf(root.hoveredSlot) >= 0)
            root.currentSlot = root.hoveredSlot.slotIndex;

        if (root.currentSlot >= live.length)
            root.currentSlot = Math.max(0, live.length - 1);
        // Re-pick the hovered slot once the hover handlers have settled.
        if (root.hoveredSlot && live.indexOf(root.hoveredSlot) < 0) {
            root.hoveredSlot = null;
            Qt.callLater(root.refreshHoveredSlot);
        }
        if (root.highlightSlot && root.slots.indexOf(root.highlightSlot) < 0)
            root.forgetSlot(root.highlightSlot);
    }

    // Clip at the frame's content rect: the title row leaves the top free, and
    // a slot still moving out is cut by the frame, not by its own slot.
    Item {
        id: slotClip

        // Clipped one frame margin wider than the content.
        x: preview.contentLeft - root.frameMargin
        y: preview.contentTop + root.titleSpace - root.frameMargin
        width: preview.contentWidth + 2 * root.frameMargin
        height: Math.max(0, preview.contentHeight - root.titleSpace
                            + 2 * root.frameMargin)
        clip: true

        // One slot instance per shown window.
        Flickable {
            id: slotView

            readonly property bool drawsAtOwnSize: preview.thumbnailReveal === 1

            enabled: preview.requestedPresentation === DockPreview.Presentation.Thumbnail
            interactive: preview.requestedPresentation === DockPreview.Presentation.Thumbnail
            // One frame margin on every side, outside the layout.
            width: preview.previewViewport.width + 2 * root.frameMargin
            height: preview.previewViewport.height + 2 * root.frameMargin
            contentWidth: preview.thumbnailContentSize.width + 2 * root.frameMargin
            contentHeight: preview.thumbnailContentSize.height + 2 * root.frameMargin
            // Only the frame clips, so a slot still moving out is not cut.
            clip: false
            boundsBehavior: Flickable.StopAtBounds
            // A settled preview sits in the content rect; while it unfolds,
            // the slots keep their dock-facing edge.
            x: (preview.contentPinToLeft || slotView.drawsAtOwnSize)
               ? 0 : slotClip.width - width
            y: slotView.drawsAtOwnSize ? 0 : slotClip.height - height
            transformOrigin: preview.contentPinToLeft ? Item.BottomLeft : Item.BottomRight
            scale: {
                if (slotView.drawsAtOwnSize)
                    return 1;
                const stripWidth = width - 2 * root.frameMargin;
                const stripHeight = height - 2 * root.frameMargin;
                if (!(stripWidth > 0) || !(stripHeight > 0))
                    return 0;
                return Math.max(0, Math.min(1,
                    (slotClip.width - 2 * root.frameMargin) / stripWidth,
                    (slotClip.height - 2 * root.frameMargin) / stripHeight));
            }
            opacity: preview.thumbnailReveal

            Item {
                id: slotLayer
                x: root.frameMargin
                y: root.frameMargin
                width: preview.thumbnailContentSize.width
                height: preview.thumbnailContentSize.height

                // One frame for the whole strip, drawn under the pictures.
                Rectangle {
                    id: hoverFrame

                    readonly property var slot: root.highlightSlot
                    // Only slide between two settled slots.
                    readonly property bool slides: preview.enableAnimation
                                                   && slot !== null && slot.progress === 1
                                                   && hoverFrame.opacity > 0
                    property rect frameRect: slot
                        ? Qt.rect(slot.x - border.width, slot.y - border.width,
                                  slot.width + 2 * border.width,
                                  slot.height + 2 * border.width)
                        : Qt.rect(0, 0, 0, 0)

                    z: -1
                    visible: slot !== null && opacity > 0
                    opacity: root.hoveredSlot !== null ? 1 : 0

                    x: frameRect.x
                    y: frameRect.y
                    width: frameRect.width
                    height: frameRect.height
                    radius: root.radius + border.width
                    color: "transparent"
                    border.width: PopoverStyle.frameBorderWidth
                    property D.Palette borderPalette: D.Palette {
                        normal {
                            common: Qt.rgba(0, 0, 0, 0.2)
                        }
                        normalDark {
                            common: Qt.rgba(1, 1, 1, 0.3)
                        }
                    }
                    border.color: D.ColorSelector.borderPalette

                    Behavior on opacity {
                        enabled: preview.enableAnimation
                        NumberAnimation {
                            duration: preview.aniDuration
                            easing.type: Easing.OutExpo
                        }
                    }

                    Behavior on frameRect {
                        enabled: hoverFrame.slides
                        PropertyAnimation {
                            duration: preview.aniDuration
                            easing.type: Easing.OutExpo
                        }
                    }
                }
            }

            HoverHandler {
                enabled: preview.requestedPresentation === DockPreview.Presentation.Thumbnail
                cursorShape: Qt.ArrowCursor
                onHoveredChanged: {
                    if (hovered)
                        return;
                    root.hoveredSlot = null;
                    preview.stopPreviewWindow();
                }
            }
        }

        // The strip scrolls sideways on a top or bottom dock and up and down on
        // the other two; the hints follow whichever axis it uses.
        DockPreviewScrollHint {
            id: scrollHint

            view: slotView
            horizontal: preview.isHorizontal
            duration: preview.aniDuration
            enableAnimation: preview.enableAnimation
            opacity: preview.thumbnailReveal
            interactive: preview.thumbnailReveal === 1
            inset: root.frameMargin
        }
    }

    Component {
        id: slotComponent

        DockPreviewSlot {
            preview: root.preview
            view: root
        }
    }

    // The title row: the app icon and title of the highlighted slot, plus the
    // close-all button.
    RowLayout {
        spacing: 4
        height: preview.titleRowHeight
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: preview.panelHorizontalPadding
            rightMargin: preview.panelHorizontalPadding
            topMargin: preview.panelVerticalPadding
        }

        Loader {
            id: appIconLoader
            readonly property var iconBuffer: root.currentWrapper?.appIcon ?? null
            active: !!iconBuffer
            visible: active
            Layout.alignment: Qt.AlignVCenter
            opacity: preview.thumbnailReveal
            sourceComponent: Component {
                BufferItem {
                    id: appIconItem
                    implicitWidth: preview.titleRowHeight
                    implicitHeight: preview.titleRowHeight
                    smooth: true

                    Binding {
                        target: appIconItem
                        property: "buffer"
                        value: appIconLoader.iconBuffer
                        when: !!appIconLoader.iconBuffer
                        restoreMode: Binding.RestoreNone
                    }
                }
            }
        }

        Item {
            id: titleTextArea
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitWidth: titleText.implicitWidth

            Text {
                id: titleText
                anchors.left: parent.left
                anchors.top: parent.top
                width: parent.width
                text: {
                    const wrapper = root.currentWrapper;
                    return wrapper && wrapper.shellSurface
                           ? wrapper.shellSurface.title : "";
                }
                font: PopoverStyle.titleFont
                color: PopoverStyle.titleColor
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
                opacity: preview.thumbnailReveal
            }
        }

        DockPreviewCloseButton {
            // Never taller than the title row it appears in.
            maximumSize: Math.min(PopoverStyle.closeButtonMaxSize, preview.titleRowHeight)
            // Only the thumbnail presentation has a close-all action; the
            // list has one per row and the tooltip has none.
            // A hovered slot shows its own close button instead.
            visible: preview.requestedPresentation === DockPreview.Presentation.Thumbnail
                     && root.hovered
                     && !root.hoveredSlot?.hovered
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            onClicked: {
                preview.closeAllWindow();
            }
        }
    }
}

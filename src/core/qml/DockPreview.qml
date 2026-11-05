// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import Waylib.Server
import Treeland
import Treeland.Protocols
import org.deepin.dtk 1.0 as D

Item {
    id: root
    property int aniDuration: 350
    property bool enableAnimation: aniDuration > 0
    // How long the closing window's picture takes to disappear. Every slot and
    // layer around it keeps using aniDuration; only the picture goes early.
    property int closeContentDuration: 100
    property int spacing: 5

    readonly property int panelHorizontalPadding: PopoverStyle.tooltipHorizontalPadding
    readonly property int panelVerticalPadding: PopoverStyle.tooltipVerticalPadding
    readonly property int screenEdgeMargin: 10
    property bool enableBlur: true
    property bool enableShadow: GraphicsInfo.api !== GraphicsInfo.Software
    property bool enableBorder: true
    // set by show()
    property var target
    property point pos
    property int direction
    function isHorizontalDirection(direction) {
        return direction !== ForeignToplevelManagerInterfaceV3.PreviewDirection.left
            && direction !== ForeignToplevelManagerInterfaceV3.PreviewDirection.right;
    }
    readonly property bool isHorizontal: root.isHorizontalDirection(root.direction)
    // auto update by binding
    readonly property var outputItem: target?.ownsOutput?.outputItem ?? null
    readonly property int screenWidth: outputItem && outputItem.width > 0 ? outputItem.width
                                                                          : 9999
    readonly property int screenHeight: outputItem && outputItem.height > 0 ? outputItem.height
                                                                            : 9999
    property size outPutSize: Qt.size(root.screenWidth, root.screenHeight)
    property point outPutPos: {
        if (!outputItem || !root.parent)
            return Qt.point(0, 0);
        return outputItem.mapToItem(root.parent, 0, 0);
    }
    // set by showTooltip(): the client-drawn tooltip surface
    property var tooltipSurface: null
    onTooltipSurfaceChanged: root.applyTooltipSurface(root.tooltipSurface)

    property var liveTooltipLayer: null
    // Natural size of the tooltip that last left.
    property size lastTooltipSize: Qt.size(0, 0)

    property bool isShowing: false

    // Content layout the popover shows. It is a mode, not liveness
    enum Mode { Tooltip, Preview }
    property int mode: DockPreview.Mode.Tooltip
    readonly property bool tooltipLayout: root.mode === DockPreview.Mode.Tooltip
    property bool stripPinToLeft: true
    // The authoritative list of windows the preview shows.
    property var desiredSurfaces: []
    onDesiredSurfacesChanged: root.syncSlots()
    // Visibility of the preview content, 0..1. It is the preview's own progress:
    // 1 while the preview is on screen, 0 while the tooltip replaces it.
    property real previewReveal: 0

    // Height of the title row. It is the space the preview layer has to leave
    // free at the top of the frame.
    readonly property real titleRowHeight: titleFontMetrics.height

    // The window the title row describes: the highlighted preview, clamped.
    readonly property var currentWrapper: {
        const surfaces = root.desiredSurfaces;
        if (surfaces.length === 0)
            return null;
        const index = Math.min(Math.max(slotView.currentSlot, 0), surfaces.length - 1);
        return surfaces[index] ?? null;
    }

    // Natural size of one preview slot: the short side is fixed (120 tall on a
    // horizontal dock, 240 wide on a vertical one), the other side follows the
    // window's aspect ratio and is clamped so one slot never dominates the strip.
    function previewSlotSize(wrapper, horizontal) {
        if (horizontal) {
            if (!wrapper || !(wrapper.height > 0))
                return Qt.size(80, 120);
            return Qt.size(Math.max(80, Math.min(240, 120 * wrapper.width / wrapper.height)), 120);
        }
        if (!wrapper || !(wrapper.width > 0))
            return Qt.size(240, 80);
        return Qt.size(240, Math.max(80, Math.min(120, 240 * wrapper.height / wrapper.width)));
    }

    // Natural size of a preview layout for a given set of surfaces. Pure, so a
    // switch can know the final frame size before any layer or slot exists.
    function contentSizeFor(wrappers, horizontal) {
        const count = wrappers ? wrappers.length : 0;
        let width = 0;
        let height = 0;
        let placed = 0;

        for (let i = 0; i < count; ++i) {
            const wrapper = wrappers[i];
            if (!wrapper)
                continue;

            const slot = root.previewSlotSize(wrapper, horizontal);
            if (placed > 0) {
                if (horizontal)
                    width += root.spacing;
                else
                    height += root.spacing;
            }
            if (horizontal) {
                width += slot.width;
                height = Math.max(height, slot.height);
            } else {
                width = Math.max(width, slot.width);
                height += slot.height;
            }
            ++placed;
        }

        return Qt.size(width, height);
    }

    readonly property size contentSize:
        root.contentSizeFor(root.desiredSurfaces, root.isHorizontal)

    readonly property size liveTooltipSize: root.liveTooltipLayer
                                            ? root.liveTooltipLayer.naturalSize
                                            : Qt.size(0, 0)

    // Natural size of the tooltip on screen, falling back to the one that just
    // left: a switch never has to measure a layer that is already fading out.
    readonly property size tooltipSize:
        root.liveTooltipSize.width > 0 ? root.liveTooltipSize : root.lastTooltipSize

    // Frame size for a preview layout on this output. A strip longer than the
    // output is not squeezed.
    function previewFrameSizeFor(contentSize) {
        const titleSpace = root.titleRowHeight + root.spacing;
        const maxContentWidth = Math.max(0, root.outPutSize.width
                                            - 2 * root.screenEdgeMargin
                                            - 2 * root.panelHorizontalPadding);
        const maxContentHeight = Math.max(0, root.outPutSize.height
                                             - 2 * root.screenEdgeMargin
                                             - 2 * root.panelVerticalPadding
                                             - titleSpace);
        return Qt.size(Math.min(contentSize.width, maxContentWidth)
                           + 2 * root.panelHorizontalPadding,
                       Math.min(contentSize.height, maxContentHeight)
                           + titleSpace + 2 * root.panelVerticalPadding);
    }

    function frameSizeFor(kind) {
        return kind === DockPreview.Mode.Tooltip
                   ? root.tooltipSize
                   : root.previewFrameSizeFor(root.contentSize);
    }

    readonly property size targetFrameSize: root.frameSizeFor(root.mode)

    // Content area the preview settles at, from the content alone.
    readonly property size previewViewport: {
        const frame = root.previewFrameSizeFor(root.contentSize);
        return Qt.size(Math.max(0, frame.width - 2 * root.panelHorizontalPadding),
                       Math.max(0, frame.height - 2 * root.panelVerticalPadding
                                        - root.titleRowHeight - root.spacing));
    }

    // The frame is the only animated geometry.
    property real frameWidth: root.targetFrameSize.width
    property real frameHeight: root.targetFrameSize.height

    // Opacity of the surface the popover paints.
    // Tooltip layers are not included.
    property real shellFade: 0

    // False only while setShowing() snaps the shell scale to its start value.
    property bool shellScaleAnimated: true

    function setShowing(showing) {
        const entersFromNothing = showing && !root.visible;
        if (showing)
            root.visible = true;
        if (entersFromNothing && !root.tooltipLayout) {
            root.shellScaleAnimated = false;
            scaleTransform.xScale = 0.1;
            root.shellScaleAnimated = true;
        }
        // The states used to carry these two numbers; the Behaviours act on the
        // assignment, so entry and exit read the same here as everywhere else.
        root.shellFade = showing ? 1 : 0;
        scaleTransform.xScale = showing ? 1 : 0.5;
        root.isShowing = showing;
    }

    // The shell fade also owns the popover's lifetime: it hides the item (and
    // drops the state it kept alive) once the fade out is over.
    Behavior on shellFade {
        SequentialAnimation {
            NumberAnimation {
                id: shellFadeAnimation
                duration: root.aniDuration
                easing.type: Easing.OutExpo
            }
            ScriptAction {
                script: {
                    if (!root.isShowing) {
                        root.visible = false;
                        root.setPreviewReveal(0, false);
                        root.tooltipSurface = null;
                        root.mode = DockPreview.Mode.Tooltip;
                        root.desiredSurfaces = [ ];
                    }
                }
            }
        }
    }

    // Progress of the preview layout, 1 whenever the preview is the current mode,
    // only animates during a tooltip <-> preview switch.
    NumberAnimation {
        id: previewRevealAnimation
        target: root
        property: "previewReveal"
        duration: root.aniDuration
        easing.type: Easing.OutExpo
        onFinished: {
            if (root.tooltipLayout)
                root.desiredSurfaces = [ ];
        }
    }

    function setPreviewReveal(value, animate) {
        if (!animate || !root.enableAnimation) {
            previewRevealAnimation.stop();
            root.previewReveal = value;
            return;
        }
        if (previewRevealAnimation.running) {
            // The same target is already animating: leave it alone.
            if (previewRevealAnimation.to === value)
                return;
        } else if (root.previewReveal === value) {
            return;
        }
        // Restart from the current value, so a switch that reverses midway does
        // not jump back to where it started.
        previewRevealAnimation.stop();
        previewRevealAnimation.from = root.previewReveal;
        previewRevealAnimation.to = value;
        previewRevealAnimation.start();
    }

    Behavior on frameWidth {
        enabled: root.visible && root.enableAnimation
        NumberAnimation {
            duration: root.aniDuration
            easing.type: Easing.OutExpo
        }
    }

    Behavior on frameHeight {
        enabled: root.visible && root.enableAnimation
        NumberAnimation {
            duration: root.aniDuration
            easing.type: Easing.OutExpo
        }
    }

    // Space the preview layer reserves for the title, growing from zero.
    readonly property real reservedTitleSpace:
        root.previewReveal * (root.titleRowHeight + root.spacing)
    // Vertical rhythm of the frame, top to bottom:
    //   panel padding | title row | spacing | preview content | panel padding
    readonly property real contentLeft: root.panelHorizontalPadding
    readonly property real contentTop: root.panelVerticalPadding + root.reservedTitleSpace
    readonly property real contentWidth:
        Math.max(0, root.frameWidth - 2 * root.panelHorizontalPadding)
    readonly property real contentHeight:
        Math.max(0, root.frameHeight - 2 * root.panelVerticalPadding - root.reservedTitleSpace)

    visible: false

    LoggingCategory {
        id: qLcDockPreview
        name: "treeland.qml.dockpreview"
        defaultLogLevel: LoggingCategory.Info
    }

    component PopoverCloseButton: D.ToolButton {
        id: closeButton

        property int maximumSize: PopoverStyle.closeButtonMaxSize
        // Rounded down to an even number so the circle keeps a whole-pixel centre.
        readonly property int diameter: 2 * Math.floor(maximumSize / 2)

        implicitWidth: diameter
        implicitHeight: diameter
        padding: 0
        icon.name: "close"
        icon.width: Math.round(diameter * 0.6)
        icon.height: Math.round(diameter * 0.6)
        // DTK paints the background as a rounded BoxPanel; half the size is a circle.
        Binding {
            target: closeButton.background
            property: "radius"
            value: closeButton.diameter / 2
            when: closeButton.background
        }
    }

    /* --- model begin --- */
    // `desiredSurfaces` is the authority. The slot instances are a view of it:
    // these helpers only edit the authority, and the change handler syncs the
    // slots afterwards.
    function removeSurface(surfaceWrapper) {
        if (!surfaceWrapper)
            return;

        const index = root.desiredSurfaces.indexOf(surfaceWrapper);
        if (index < 0)
            return;

        // Copy instead of splice: the assignment is what triggers the change.
        const surfaces = root.desiredSurfaces.slice();
        surfaces.splice(index, 1);
        root.desiredSurfaces = surfaces;
    }

    function activateWindow(surfaceWrapper) {
        if (!surfaceWrapper)
            return;
        console.debug(qLcDockPreview, "activate preview window: ", surfaceWrapper)
        WaylandServer.forceActivateSurface(surfaceWrapper)
        ForeignToplevelManagerInterfaceV3.leaveDockPreview(root.target.shellSurface.surface)
        root.close();
    }

    function previewWindow(surfaceWrapper) {
        if (!surfaceWrapper)
            return;
        console.debug(qLcDockPreview, "start preview window: ", surfaceWrapper)
        WaylandServer.workspace.startPreviewing(surfaceWrapper);
    }

    function stopPreviewWindow() {
        console.debug(qLcDockPreview, "stop preview window")
        WaylandServer.workspace.stopPreviewing();
    }

    function closeAllWindow() {
        console.debug(qLcDockPreview, "close all preview windows")
        // Only ask; every slot goes away with the window it shows.
        for (const surface of root.desiredSurfaces) {
            if (surface && surface.shellSurface)
                surface.shellSurface.close();
        }
    }

    function closeSpecialWindow(surfaceWrapper) {
        if (!surfaceWrapper)
            return;

        console.debug(qLcDockPreview, "close a special window: ", surfaceWrapper)
        // Only ask; the slot goes away when the window really does.
        if (surfaceWrapper.shellSurface)
            surfaceWrapper.shellSurface.close()
    }

    /* --- slots begin --- */
    // Natural size of the content in the slot.
    function naturalContentSize(slotRect, sourceWidth, sourceHeight) {
        if (!slotRect || !(slotRect.width > 0) || !(slotRect.height > 0)
                || !(sourceWidth > 0) || !(sourceHeight > 0))
            return Qt.size(0, 0);

        const scale = Math.min(slotRect.width / sourceWidth,
                               slotRect.height / sourceHeight);
        return Qt.size(Math.max(0, sourceWidth * scale - 4),
                       Math.max(0, sourceHeight * scale - 4));
    }

    // Where every window sits inside the strip, in the strip's coordinates.
    // Pure: the frame size and the slot targets both read the same layout.
    function slotRects(wrappers) {
        const rects = [ ];
        let offset = 0;
        for (let i = 0; i < wrappers.length; ++i) {
            const size = root.previewSlotSize(wrappers[i], root.isHorizontal);
            if (root.isHorizontal) {
                rects.push(Qt.rect(offset, 0, size.width, size.height));
                offset += size.width + root.spacing;
            } else {
                rects.push(Qt.rect(0, offset, size.width, size.height));
                offset += size.height + root.spacing;
            }
        }
        return rects;
    }

    // The hover border belongs to a slot, but only the slot under the pointer
    // shows it.
    function updateHoverBorder() {
        if (!slotView)
            return;
        const slots = slotView.slots;
        for (let i = 0; i < slots.length; ++i)
            slots[i].borderOpacity = slots[i] === slotView.hoveredSlot ? 1 : 0;
    }

    // Pick the slot under the pointer again after the one it was on went away.
    function refreshHoveredSlot() {
        if (!slotView)
            return;
        const slots = slotView.slots;
        let hovered = null;
        for (let i = 0; i < slots.length; ++i) {
            if (!slots[i].collapsing && slots[i].hovered) {
                hovered = slots[i];
                break;
            }
        }
        slotView.hoveredSlot = hovered;
        root.updateHoverBorder();
    }

    function dropSlot(slot) {
        if (!slot)
            return;

        const slots = slotView.slots;
        const index = slots.indexOf(slot);
        if (index >= 0) {
            const next = slots.slice();
            next.splice(index, 1);
            slotView.slots = next;
        }

        slot.destroy();
    }

    function clearSlots() {
        if (!slotView)
            return;

        const slots = slotView.slots;
        slotView.slots = [ ];
        slotView.hoveredSlot = null;
        for (let i = 0; i < slots.length; ++i)
            slots[i].destroy();
    }

    // Bring the slot instances in line with `desiredSurfaces`.
    function syncSlots() {
        if (!slotView || !slotView.contentItem)
            return;

        const wrappers = [ ];
        const surfaces = root.desiredSurfaces;
        for (let i = 0; i < surfaces.length; ++i) {
            if (surfaces[i])
                wrappers.push(surfaces[i]);
        }

        if (wrappers.length === 0) {
            slotView.currentSlot = 0;
            if (root.isShowing && !root.tooltipLayout) {
                root.close();
                return;
            }
            root.clearSlots();
            return;
        }

        // Target rects: where every window should sit on the strip.
        const rects = root.slotRects(wrappers);
        // Old instances: the live slots, then the collapsing ones. A
        // collapsing slot can still take its window back until it is gone.
        const previous = slotView.slots;
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

            const created = previewSlotComponent.createObject(slotLayer);
            if (!created) {
                console.warn(qLcDockPreview, "failed to create a preview slot");
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

        // Bookkeeping: strip order, z, current slot, hover.
        slotView.slots = live.concat(nextRetiring);

        for (let i = 0; i < live.length; ++i) {
            live[i].slotIndex = i;
            // Later slots stay below, so a new one comes out from behind.
            live[i].z = slotView.slotZBase - i;
        }
        for (let i = 0; i < nextRetiring.length; ++i)
            nextRetiring[i].z = 0;

        // The title row follows its slot when the strip renumbers.
        if (slotView.hoveredSlot && live.indexOf(slotView.hoveredSlot) >= 0)
            slotView.currentSlot = slotView.hoveredSlot.slotIndex;

        if (slotView.currentSlot >= live.length)
            slotView.currentSlot = Math.max(0, live.length - 1);
        // Re-pick the hovered slot once the hover handlers have settled.
        if (slotView.hoveredSlot && live.indexOf(slotView.hoveredSlot) < 0) {
            slotView.hoveredSlot = null;
            Qt.callLater(root.refreshHoveredSlot);
        }
        root.updateHoverBorder();
    }
    /* --- slots end --- */

    function show(surfaces, target, pos, direction) {
        console.info(qLcDockPreview, "start show windows: ", surfaces)
        const wasVisible = root.visible;
        const wasOpen = root.isShowing;
        const wasTooltip = root.tooltipLayout;
        root.parent = target.parent;

        // We want keep the strip anchored to the left as much as possible, many
        // animations rely on it. Only a `dock preview` <-> `tooltip` switch may
        // anchor it to the other side.
        if (root.isShowing && wasTooltip)
            root.stripPinToLeft = root.pos?.x - pos?.x <= 0;
        else
            root.stripPinToLeft = true

        root.pos = pos;
        root.direction = direction;
        root.target = target;
        root.mode = DockPreview.Mode.Preview;
        if (!wasOpen || wasTooltip) {
            slotView.currentSlot = 0;
            slotView.hoveredSlot = null;
        }
        // Drop empty entries once, at the only entry that takes an external list.
        const windows = [];
        for (let i = 0; i < surfaces.length; ++i) {
            if (surfaces[i])
                windows.push(surfaces[i]);
        }
        root.desiredSurfaces = windows;
        // An empty list has nothing to reveal; syncSlots already closed it.
        if (windows.length === 0)
            return;
        slotView.contentX = 0;
        slotView.contentY = 0;
        // A popover that is already on screen switches with a curve; one that is
        // entering from nothing lands on the final layout right away.
        root.setPreviewReveal(1, wasVisible);
        root.setShowing(true);
        root.tooltipSurface = null;
    }

    // Every tooltip the dock stops showing becomes a frozen layer.
    function demoteTooltipLayer(layer) {
        if (!layer)
            return;

        // Snapshot the size before clearing the live layer: tooltipSize falls
        // back to lastTooltipSize whenever there is no live one, so this keeps
        // the frame target from dipping during a switch.
        const size = layer.naturalSize;
        if (size.width > 0 && size.height > 0)
            root.lastTooltipSize = size;
        if (root.liveTooltipLayer === layer)
            root.liveTooltipLayer = null;
        layer.freeze();
        layer.fadeOut();
    }

    function createLiveTooltip(surface) {
        const layer = tooltipLayerComponent.createObject(contentHost);
        if (!layer) {
            console.warn(qLcDockPreview, "failed to create the tooltip layer");
            return;
        }
        const item = tooltipSurfaceComponent.createObject(layer);
        if (!item) {
            console.warn(qLcDockPreview, "failed to create the tooltip surface item");
            layer.destroy();
            return;
        }
        // Set the surface on an already completed item, so it is initialized the
        // same way the previously static item was, and has its size right away.
        item.surface = surface;
        layer.surfaceItem = item;
        root.liveTooltipLayer = layer;
        layer.fadeIn();
    }

    function applyTooltipSurface(surface) {
        // Only the change handler calls this.
        const live = root.liveTooltipLayer;
        if (surface)
            root.createLiveTooltip(surface);
        root.demoteTooltipLayer(live);
    }

    function showTooltip(surface, target, pos, direction) {
        console.info(qLcDockPreview, "start show tooltip: ", surface, target, pos, direction)
        const wasVisible = root.visible;
        root.parent = target.parent;
        if (root.isShowing && !root.tooltipLayout)
            root.stripPinToLeft = root.pos?.x - pos?.x >= 0;
        else
            root.stripPinToLeft = true

        root.pos = pos;
        root.direction = direction;
        root.target = target;
        // Publish the surface before the mode: tooltipSize reads the live layer,
        // and switching mode first would let the frame target fall back to the
        // previous tooltip for a moment.
        root.tooltipSurface = surface;
        root.mode = DockPreview.Mode.Tooltip;
        // Same as show(): a switch animates the preview away, an entry from
        // nothing starts with the preview already collapsed.
        root.setPreviewReveal(0, wasVisible);
        root.setShowing(true);
        if (!root.enableAnimation)
            root.desiredSurfaces = [ ];
    }

    function close() {
        console.info(qLcDockPreview, "stop dock preview")
        root.setShowing(false);
        root.tooltipSurface = null;
    }
    /* --- model end --- */

    /* --- global position begin --- */
    implicitWidth: root.targetFrameSize.width
    implicitHeight: root.targetFrameSize.height

    // Where the frame settles in the parent's coordinates.
    readonly property real unadjustedFrameX: {
        if (!root.target)
            return 0;

        const frameWidth = root.targetFrameSize.width;
        switch (root.direction) {
        case ForeignToplevelManagerInterfaceV3.PreviewDirection.left:
            return root.target.x + root.pos.x;
        case ForeignToplevelManagerInterfaceV3.PreviewDirection.right:
            return root.target.x + root.pos.x - frameWidth;
        default: // bottom / top: the frame's center follows the dock icon
            return root.target.x + root.pos.x - frameWidth / 2;
        }
    }
    readonly property real unadjustedFrameY: {
        if (!root.target)
            return 0;

        const frameHeight = root.targetFrameSize.height;
        switch (root.direction) {
        case ForeignToplevelManagerInterfaceV3.PreviewDirection.bottom:
            return root.target.y + root.pos.y - frameHeight;
        case ForeignToplevelManagerInterfaceV3.PreviewDirection.top:
            return root.target.y + root.pos.y;
        default: // left / right: the frame's center follows the dock icon
            return root.target.y + root.pos.y - frameHeight / 2;
        }
    }

    // Keep the settled frame inside the output, with a margin on every side.
    readonly property real horizontalCenterOffsetAdjust: {
        const leftSpace = root.unadjustedFrameX - root.outPutPos.x;
        if (leftSpace < 0)
            return -leftSpace + root.screenEdgeMargin;

        const rightSpace = root.outPutPos.x + root.outPutSize.width
                           - (root.unadjustedFrameX + root.targetFrameSize.width);
        if (rightSpace < 0)
            return rightSpace - root.screenEdgeMargin;

        return 0;
    }
    readonly property real verticalCenterOffsetAdjust: {
        const topSpace = root.unadjustedFrameY - root.outPutPos.y;
        if (topSpace < 0)
            return -topSpace + root.screenEdgeMargin;

        const bottomSpace = root.outPutPos.y + root.outPutSize.height
                            - (root.unadjustedFrameY + root.targetFrameSize.height);
        if (bottomSpace < 0)
            return bottomSpace - root.screenEdgeMargin;

        return 0;
    }

    states: [
        State {
            name: "dock_bottom"
            when: direction === ForeignToplevelManagerInterfaceV3.PreviewDirection.bottom
            AnchorChanges {
                target: root
                anchors.horizontalCenter: root.target?.horizontalCenter
                anchors.bottom: root.target?.top
            }
            AnchorChanges {
                target: background
                anchors.horizontalCenter: root.horizontalCenter
                anchors.bottom: root.bottom
                // Pin the background's dock-facing edge to root's.
            }
            PropertyChanges {
                target: root
                anchors.horizontalCenterOffset: root.pos.x - root.target?.width / 2 + horizontalCenterOffsetAdjust
                anchors.bottomMargin: -root.pos.y
                restoreEntryValues: false
            }
            PropertyChanges {
                target: scaleTransform
                origin.x: root.frameWidth / 2
                origin.y: root.frameHeight
            }
        },
        State {
            name: "dock_left"
            when: direction === ForeignToplevelManagerInterfaceV3.PreviewDirection.left
            AnchorChanges {
                target: root
                anchors.verticalCenter: root.target?.verticalCenter
                anchors.left: root.target?.left
            }
            AnchorChanges {
                target: background
                anchors.verticalCenter: root.verticalCenter
                anchors.left: root.left
            }
            PropertyChanges {
                target: root
                anchors.verticalCenterOffset: root.pos.y - root.target?.height / 2 + verticalCenterOffsetAdjust
                anchors.leftMargin: root.pos.x
                restoreEntryValues: false
            }
            PropertyChanges {
                target: scaleTransform
                origin.x: 0
                origin.y: root.frameHeight / 2
            }
        },
        State {
            name: "dock_top"
            when: direction === ForeignToplevelManagerInterfaceV3.PreviewDirection.top
            AnchorChanges {
                target: root
                anchors.horizontalCenter: root.target?.horizontalCenter
                anchors.top: root.target?.top
            }
            AnchorChanges {
                target: background
                anchors.horizontalCenter: root.horizontalCenter
                anchors.top: root.top
            }
            PropertyChanges {
                target: root
                anchors.horizontalCenterOffset: root.pos.x - root.target?.width / 2 + horizontalCenterOffsetAdjust
                anchors.topMargin: root.pos.y
                restoreEntryValues: false
            }
            PropertyChanges {
                target: scaleTransform
                origin.x: root.frameWidth / 2
                origin.y: 0
            }
        },
        State {
            name: "dock_right"
            when: direction === ForeignToplevelManagerInterfaceV3.PreviewDirection.right
            AnchorChanges {
                target: root
                anchors.verticalCenter: root.target?.verticalCenter
                anchors.right: root.target?.left
            }
            AnchorChanges {
                target: background
                anchors.verticalCenter: root.verticalCenter
                anchors.right: root.right
            }
            PropertyChanges {
                target: root
                anchors.verticalCenterOffset: root.pos.y - root.target?.height / 2 + verticalCenterOffsetAdjust
                anchors.rightMargin: -root.pos.x
                restoreEntryValues: false
            }
            PropertyChanges {
                target: scaleTransform
                origin.x: root.frameWidth
                origin.y: root.frameHeight / 2
            }
        }
    ]

    Behavior on anchors.horizontalCenterOffset {
        enabled: root.visible && root.enableAnimation
        NumberAnimation {
            duration: root.aniDuration
            easing.type: Easing.OutExpo
        }
    }

    Behavior on anchors.verticalCenterOffset {
        enabled: root.visible && root.enableAnimation
        NumberAnimation {
            duration: root.aniDuration
            easing.type: Easing.OutExpo
        }
    }
    /* --- global position end --- */

    // Line height of the title font, measured without a real string.
    FontMetrics {
        id: titleFontMetrics
        font: PopoverStyle.titleFont
    }

    // Clip for the strip: the frame's content rect as it is drawn right now.
    Item {
        id: stripClip
        parent: contentHost
        x: root.contentLeft
        y: root.contentTop
        width: root.contentWidth
        height: root.contentHeight
        clip: true
    }

    // The preview strip: one slot instance per shown window.
    Flickable {
        id: slotView
        parent: stripClip

        property int radius: 5
        // Topmost z of the preview slots.
        readonly property int slotZBase: 10000
        // Slots in strip order: live ones first, then the collapsing ones.
        property var slots: []
        // List row the title follows.
        property int currentSlot: 0
        // The slot under the pointer; it shows the border.
        property var hoveredSlot: null
        // The strip owns the frame's content rect once the preview has
        // settled; only a mode switch fits it into the frame while it unfolds.
        readonly property bool lockScale: !root.tooltipLayout
                                          && root.previewReveal === 1

        enabled: !root.tooltipLayout
        interactive: !root.tooltipLayout
        width: root.previewViewport.width
        height: root.previewViewport.height
        contentWidth: root.contentSize.width
        contentHeight: root.contentSize.height
        // Only the frame clips, so a slot still moving out is not cut.
        clip: false
        boundsBehavior: Flickable.StopAtBounds
        // A settled preview sits in the frame's content rect; while a mode
        // switch unfolds it, the strip keeps its dock-facing edge.
        x: (root.stripPinToLeft || slotView.lockScale) ? 0 : stripClip.width - width
        y: slotView.lockScale ? 0 : stripClip.height - height
        transformOrigin: root.stripPinToLeft ? Item.BottomLeft : Item.BottomRight
        scale: {
            if (slotView.lockScale)
                return 1;
            if (!(width > 0) || !(height > 0))
                return 0;
            return Math.max(0, Math.min(1, root.contentWidth / width,
                                         root.contentHeight / height));
        }
        // Reveal covers mode switches; the shell fade covers enter and leave.
        opacity: root.previewReveal * root.shellFade

        // Coordinate space of the layout: slots are created into it.
        Item {
            id: slotLayer
            width: root.contentSize.width
            height: root.contentSize.height
        }

        HoverHandler {
            enabled: !root.tooltipLayout
            cursorShape: Qt.ArrowCursor
            onHoveredChanged: {
                if (hovered)
                    return;
                slotView.hoveredSlot = null;
                root.stopPreviewWindow();
                root.updateHoverBorder();
            }
        }
    }

    Component {
        id: previewSlotComponent

        Item {
            id: slot

            property var wrapper: null
            property rect fromRect: Qt.rect(0, 0, 0, 0)
            property rect toRect: Qt.rect(0, 0, 0, 0)
            property real progress: 1
            // Whole-slot opacity: a slot is created below the strip, fades in
            // as it slides out, and fades out while it collapses.
            property real reveal: 1
            // The slot owns its border; the strip only decides which slot shows
            // it.
            property real borderOpacity: 0
            // Content cross-fade, like the tooltip layers.
            property real contentOpacity: 1
            // Pictures that left this slot and are still fading out, one per
            // switch.
            property var outgoingLayers: []
            // Size every layer inside the slot is drawn at. A switch animates
            // it; the slot box never changes it.
            property size contentSize: Qt.size(0, 0)
            // Set by setContentSize(): animate this write or snap it.
            property bool contentSizeAnimated: false
            property bool collapsing: false
            property int slotIndex: -1
            // True while the pointer is over this slot's box, so the strip can
            // pick the survivor after a move between slots.
            readonly property bool hovered: slotHover.hovered

            Behavior on contentSize {
                PropertyAnimation {
                    duration: slot.contentSizeAnimated ? root.aniDuration : 0
                    easing.type: Easing.OutExpo
                }
            }

            objectName: "previewSlot"
            x: fromRect.x + (toRect.x - fromRect.x) * progress
            y: fromRect.y + (toRect.y - fromRect.y) * progress
            width: fromRect.width + (toRect.width - fromRect.width) * progress
            height: fromRect.height + (toRect.height - fromRect.height) * progress
            opacity: reveal

            function currentRect() {
                return Qt.rect(x, y, width, height)
            }

            function equals(a, b) {
                return a.x === b.x && a.y === b.y
                        && a.width === b.width && a.height === b.height;
            }

            function setContentSize(newWrapper, rect, animated) {
                contentSizeAnimated = animated && root.enableAnimation;
                contentSize = root.naturalContentSize(rect,
                                                      newWrapper ? newWrapper.width : 0,
                                                      newWrapper ? newWrapper.height : 0);
            }

            // The only place that writes the geometry. The new animation always
            // starts from the rect the slot is drawn at, not from where the old
            // animation was heading.
            function startMove(rect) {
                const current = currentRect();
                if (!root.enableAnimation) {
                    fromRect = rect;
                    toRect = rect;
                    progress = 1;
                    return;
                }
                fromRect = current;
                toRect = rect;
                progress = 0;
                moveAnimation.restart();
            }

            function enter(newWrapper, startRect, rect, instant) {
                wrapper = newWrapper;
                setContentSize(newWrapper, rect, false);
                contentOpacity = 1;
                clearOutgoing();
                if (instant || !root.enableAnimation) {
                    fromRect = rect;
                    toRect = rect;
                    progress = 1;
                    reveal = 1;
                    return;
                }
                fromRect = startRect;
                toRect = rect;
                progress = 0;
                moveAnimation.restart();
                // A slot that appears fades in from nothing while it slides out
                // from behind the last one, so it never pops into existence.
                reveal = 0;
                revealAnimation.stop();
                revealAnimation.from = 0;
                revealAnimation.to = 1;
                revealAnimation.restart();
            }

            function retarget(newWrapper, rect) {
                if (newWrapper !== wrapper)
                    swapContent(newWrapper, rect);
                else
                    setContentSize(newWrapper, rect, true);
                // Already heading there: leave the running animation alone.
                if (equals(rect, toRect))
                    return;
                startMove(rect);
            }

            // A closing slot keeps its own picture. It slides into the rect it
            // is given, starting from where it is drawn, and only disappears
            // once that is done. The picture is the one thing that goes early;
            // the slot and every layer around it stay on the normal duration.
            function collapseTo(rect) {
                collapsing = true;
                // Freeze the picture while the window can still provide it;
                closeSnapshot.scheduleUpdate();
                borderOpacity = 0;
                revealAnimation.stop();
                if (root.enableAnimation) {
                    contentReveal.stop();
                    closeContentFade.from = contentOpacity;
                    closeContentFade.to = 0;
                    closeContentFade.restart();
                }
                startMove(rect);
                if (!root.enableAnimation) {
                    root.dropSlot(slot);
                    return false;
                }
                revealAnimation.from = reveal;
                revealAnimation.to = 0;
                revealAnimation.restart();
                return true;
            }

            // The list took this slot back before it was gone: turn around from
            // where it is drawn right now.
            function revive(newWrapper, rect) {
                collapsing = false;
                closeContentFade.stop();
                if (newWrapper !== wrapper) {
                    swapContent(newWrapper, rect);
                } else {
                    setContentSize(newWrapper, rect, true);
                    // The picture may have started to disappear.
                    if (contentOpacity < 1) {
                        if (!root.enableAnimation) {
                            contentOpacity = 1;
                        } else {
                            contentReveal.stop();
                            contentReveal.from = contentOpacity;
                            contentReveal.to = 1;
                            contentReveal.restart();
                        }
                    }
                }
                if (!equals(rect, toRect) || progress < 1)
                    startMove(rect);
                revealAnimation.stop();
                if (!root.enableAnimation) {
                    reveal = 1;
                    return;
                }
                revealAnimation.from = reveal;
                revealAnimation.to = 1;
                revealAnimation.restart();
            }

            // Move the picture on screen to a layer of its own.
            function demoteContent() {
                if (!wrapper)
                    return;

                const layer = outgoingLayerComponent.createObject(contentClip);
                if (!layer) {
                    console.warn(qLcDockPreview, "failed to create a slot layer");
                    return;
                }
                layer.wrapper = wrapper;
                layer.opacity = contentOpacity;
                outgoingLayers.push(layer);
                layer.fadeOut();
            }

            function dropOutgoing(layer) {
                if (!layer)
                    return;

                const index = outgoingLayers.indexOf(layer);
                if (index >= 0)
                    outgoingLayers.splice(index, 1);
                layer.destroy();
            }

            // Drop every picture the slot used to show.
            function clearOutgoing() {
                const layers = outgoingLayers;
                outgoingLayers = [ ];
                for (let i = 0; i < layers.length; ++i) {
                    if (layers[i])
                        layers[i].destroy();
                }
            }

            // The old content leaves on its own snapshot layer, the new one
            // fades in.
            function swapContent(newWrapper, slotRect) {
                demoteContent();
                wrapper = newWrapper;
                setContentSize(newWrapper, slotRect, true);

                contentReveal.stop();
                if (!root.enableAnimation) {
                    contentOpacity = 1;
                    clearOutgoing();
                    return;
                }
                contentOpacity = 0;
                contentReveal.from = 0;
                contentReveal.to = 1;
                contentReveal.restart();
            }

            Rectangle {
                anchors.fill: parent
                radius: slotView.radius
                color: PopoverStyle.itemBackground
            }

            Item {
                id: contentClip
                anchors.fill: parent
                clip: true

                ShaderEffectSource {
                    id: contentSource
                    anchors.centerIn: parent
                    implicitWidth: slot.contentSize.width
                    implicitHeight: slot.contentSize.height
                    opacity: slot.contentOpacity
                    visible: slot.wrapper !== null
                    live: true
                    hideSource: false
                    smooth: true
                    sourceItem: slot.wrapper
                }

                // The window may be destroyed before the collapse ends, so the
                // closing slot shows a frame frozen while it still had one.
                ShaderEffectSource {
                    id: closeSnapshot
                    anchors.centerIn: parent
                    implicitWidth: slot.contentSize.width
                    implicitHeight: slot.contentSize.height
                    // The same early fade as the live picture, so a window that
                    // is already gone does not keep it on screen.
                    opacity: slot.contentOpacity
                    visible: slot.collapsing
                    live: false
                    hideSource: false
                    smooth: true
                    z: 1
                    sourceItem: slot.collapsing ? contentSource : null
                }
            }

            Component {
                id: outgoingLayerComponent

                ShaderEffectSource {
                    id: outgoingLayer
                    anchors.centerIn: parent
                    implicitWidth: slot.contentSize.width
                    implicitHeight: slot.contentSize.height
                    // Snapshot once; the layer only fades from here on.
                    live: false
                    hideSource: false
                    smooth: true
                    z: 1
                    property var wrapper: null
                    sourceItem: wrapper

                    function fadeOut() {
                        if (!root.enableAnimation) {
                            slot.dropOutgoing(outgoingLayer);
                            return;
                        }
                        fadeAnimation.from = opacity;
                        fadeAnimation.to = 0;
                        fadeAnimation.restart();
                    }

                    NumberAnimation {
                        id: fadeAnimation
                        target: outgoingLayer
                        property: "opacity"
                        duration: root.aniDuration
                        easing.type: Easing.OutExpo
                        onFinished: slot.dropOutgoing(outgoingLayer)
                    }

                    Connections {
                        target: outgoingLayer.wrapper
                        function onAboutToBeInvalidated() {
                            slot.dropOutgoing(outgoingLayer);
                        }
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: slotView.radius + 4
                color: "transparent"
                border.width: 4
                opacity: slot.borderOpacity
                property D.Palette borderPalette: D.Palette {
                    normal {
                        common: Qt.rgba(0, 0, 0, 0.2)
                    }
                    normalDark {
                        common: Qt.rgba(1, 1, 1, 0.3)
                    }
                }
                border.color: D.ColorSelector.borderPalette
            }

            Connections {
                target: slot.wrapper
                function onAboutToBeInvalidated() {
                    root.removeSurface(slot.wrapper);
                }
            }

            HoverHandler {
                id: slotHover
                enabled: !slot.collapsing && !root.tooltipLayout
                cursorShape: Qt.ArrowCursor
                onHoveredChanged: {
                    if (!hovered)
                        return;
                    root.previewWindow(slot.wrapper);
                    slotView.currentSlot = slot.slotIndex;
                    slotView.hoveredSlot = slot;
                    root.updateHoverBorder();
                }
            }

            TapHandler {
                onTapped: root.activateWindow(slot.wrapper)
            }

            // One close button per slot, shown while hovering it.
            PopoverCloseButton {
                anchors {
                    top: parent.top
                    right: parent.right
                    topMargin: 10
                    rightMargin: 10
                }
                visible: slotHover.hovered && !slot.collapsing
                onClicked: {
                    // Capture the wrapper now: the deferred callback must not
                    // read the slot's content again, it may have been reused.
                    const target = slot.wrapper;
                    Qt.callLater(()=>root.closeSpecialWindow(target));
                }
            }

            NumberAnimation {
                id: moveAnimation
                target: slot
                property: "progress"
                from: 0
                to: 1
                duration: root.aniDuration
                easing.type: Easing.OutExpo
            }

            Behavior on borderOpacity {
                NumberAnimation {
                    duration: root.aniDuration
                    easing.type: Easing.OutExpo
                }
            }

            NumberAnimation {
                id: revealAnimation
                target: slot
                property: "reveal"
                duration: root.aniDuration
                easing.type: Easing.OutExpo
                onFinished: {
                    if (slot.collapsing)
                        root.dropSlot(slot);
                }
            }

            NumberAnimation {
                id: contentReveal
                target: slot
                property: "contentOpacity"
                duration: root.aniDuration
                easing.type: Easing.OutExpo
            }

            // Only the closing window's picture goes early. Every layer around
            // it keeps using aniDuration.
            NumberAnimation {
                id: closeContentFade
                target: slot
                property: "contentOpacity"
                duration: root.closeContentDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Rectangle {
        id: background
        z: root.z
        // Only the frame geometry.
        color: "transparent"
        width: root.frameWidth
        height: root.frameHeight

        radius: PopoverStyle.radius
        clip: false
        parent: root.parent
        visible: root.visible

        // Host of every content layer. Each layer owns its opacity, so nothing
        // here multiplies into the shell's fade. The shell paints below them.
        Item {
            id: contentHost
            anchors.fill: parent
            z: 0
        }

        // The group fades on its own value.
        Rectangle {
            id: shell
            anchors.fill: parent
            z: -1
            opacity: root.shellFade
            clip: false
            color: PopoverStyle.tintFor(root.enableBlur && backgroundBlur.blurEnabled)
            radius: background.radius

            XdgShadow {
                visible: root.enableShadow
                cornerRadius: background.radius
                shadowBlur: PopoverStyle.shadowBlur
                shadowOffsetX: 0
                shadowOffsetY: PopoverStyle.shadowOffsetY
                shadowColor: PopoverStyle.shadowColor
            }

            Blur {
                id: backgroundBlur
                anchors.fill: parent
                visible: root.enableBlur
                radius: background.radius
            }

            Border {
                anchors.fill: parent
                visible: root.enableBorder
                radius: background.radius
                outsideColor: PopoverStyle.borderOutsideColor
                insideColor: PopoverStyle.borderInsideColor
            }
        }

        HoverHandler {
            id: titleHover
            enabled: root.desiredSurfaces.length !== 0
            onHoveredChanged: {
                if (!hovered) {
                    ForeignToplevelManagerInterfaceV3.leaveDockPreview(root.target.shellSurface.surface)
                } else {
                    ForeignToplevelManagerInterfaceV3.enterDockPreview(root.target.shellSurface.surface)
                }
            }
            cursorShape: Qt.ArrowCursor
        }

        RowLayout {
            id: headLayout
            spacing: 4
            parent: shell
            height: root.titleRowHeight
            anchors {
                left: shell.left
                right: shell.right
                top: shell.top
                leftMargin: root.panelHorizontalPadding
                rightMargin: root.panelHorizontalPadding
                topMargin: root.panelVerticalPadding
            }

            Loader {
                id: appIconLoader
                readonly property var iconBuffer: root.currentWrapper?.appIcon ?? null
                active: !!iconBuffer
                visible: active
                Layout.alignment: Qt.AlignVCenter
                opacity: root.previewReveal
                sourceComponent: Component {
                    BufferItem {
                        id: appIconItem
                        implicitWidth: root.titleRowHeight
                        implicitHeight: root.titleRowHeight
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
                    opacity: root.previewReveal
                }
            }

            PopoverCloseButton {
                // Never taller than the title row it appears in.
                maximumSize: Math.min(PopoverStyle.closeButtonMaxSize, root.titleRowHeight)
                // Only the preview state has a close-all action: in the tooltip
                // state the button would take width from the text.
                visible: !root.tooltipLayout && titleHover.hovered
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                onClicked: {
                    root.closeAllWindow()
                }
            }
        }

        Component {
            id: tooltipSurfaceComponent
            SurfaceItem {
                flags: SurfaceItem.RejectEvent
                // Its natural size, never stretched; the owning layer is what gets
                // positioned and compressed.
                width: implicitWidth
                height: implicitHeight
            }
        }

        // One layer per shown tooltip surface.
        Component {
            id: tooltipLayerComponent

            Item {
                id: layer
                z: 1
                // The frame this layer shows.
                property Item surfaceItem: null
                property bool fadingOut: false

                readonly property size naturalSize: surfaceItem
                                                     ? Qt.size(surfaceItem.implicitWidth,
                                                               surfaceItem.implicitHeight)
                                                     : Qt.size(0, 0)

                // Where the layer sits inside the frame, for the current
                // direction. Position and the scale origin both read the same
                // animated frame on root, so they cannot disagree.
                readonly property real layerX: {
                    switch (root.direction) {
                    case ForeignToplevelManagerInterfaceV3.PreviewDirection.left:
                        return 0;
                    case ForeignToplevelManagerInterfaceV3.PreviewDirection.right:
                        return root.frameWidth - width;
                    default: // top / bottom: centred on the dock icon
                        return (root.frameWidth - width) / 2;
                    }
                }

                readonly property real layerY: {
                    switch (root.direction) {
                    case ForeignToplevelManagerInterfaceV3.PreviewDirection.top:
                        return 0;
                    case ForeignToplevelManagerInterfaceV3.PreviewDirection.bottom:
                        return root.frameHeight - height;
                    default: // left / right: centred on the dock icon
                        return (root.frameHeight - height) / 2;
                    }
                }

                // Every layer shrinks towards the edge the tooltip is glued to.
                readonly property point scaleOrigin: {
                    switch (root.direction) {
                    case ForeignToplevelManagerInterfaceV3.PreviewDirection.left:
                        return Qt.point(0, height / 2);
                    case ForeignToplevelManagerInterfaceV3.PreviewDirection.right:
                        return Qt.point(width, height / 2);
                    case ForeignToplevelManagerInterfaceV3.PreviewDirection.top:
                        return Qt.point(width / 2, 0);
                    default: // bottom
                        return Qt.point(width / 2, height);
                    }
                }
                // Fit into the frame without ever stretching it.
                readonly property real compressionX:
                    width > 0 && root.frameWidth > 0 ? Math.min(1, root.frameWidth / width) : 1
                readonly property real compressionY:
                    height > 0 && root.frameHeight > 0 ? Math.min(1, root.frameHeight / height) : 1

                visible: surfaceItem !== null
                width: naturalSize.width
                height: naturalSize.height

                x: layer.layerX
                y: layer.layerY

                transform: Scale {
                    origin.x: layer.scaleOrigin.x
                    origin.y: layer.scaleOrigin.y
                    xScale: layer.compressionX
                    yScale: layer.compressionY
                }

                NumberAnimation {
                    id: fadeAnimation
                    target: layer
                    property: "opacity"
                    duration: root.aniDuration
                    easing.type: Easing.OutExpo
                    onFinished: {
                        if (layer.fadingOut)
                            layer.destroy();
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
                    if (!root.enableAnimation) {
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
                    if (!root.enableAnimation) {
                        layer.destroy();
                        return;
                    }
                    fadeAnimation.from = opacity;
                    fadeAnimation.to = 0;
                    fadeAnimation.restart();
                }
            }
        }

        transform: Scale {
            id: scaleTransform
            xScale: 0.5
            yScale: xScale

            Behavior on xScale {
                enabled: root.shellScaleAnimated && root.enableAnimation
                NumberAnimation {
                    id: scaleAnimation
                    duration: root.aniDuration
                    easing.type: Easing.OutExpo
                }
            }
        }
    }
}

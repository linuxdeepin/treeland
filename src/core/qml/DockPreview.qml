// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

import Treeland
import Treeland.Protocols

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
    onTooltipSurfaceChanged: tooltip.applySurface(root.tooltipSurface)

    property bool isShowing: false

    // Content layout the popover shows. It is a mode, not liveness
    enum Mode { Tooltip, Preview }
    property int mode: DockPreview.Mode.Tooltip
    readonly property bool tooltipLayout: root.mode === DockPreview.Mode.Tooltip
    // Which side of the frame the preview content stays glued to.
    property bool contentPinToLeft: true
    // The authoritative list of windows the preview shows.
    property var desiredSurfaces: []
    onDesiredSurfacesChanged: root.syncSurfaces()
    // Visibility of the preview content, 0..1. It is the preview's own progress:
    // 1 while the preview is on screen, 0 while the tooltip replaces it.
    property real previewReveal: 0

    // Height of the title row. It is the space the preview layer has to leave
    // free at the top of the frame.
    readonly property real titleRowHeight: titleFontMetrics.height

    // Natural size of one preview slot: the short side is fixed (120 tall on a
    // horizontal dock, 240 wide on a vertical one), the other side follows the
    // window's aspect ratio and is clamped so one slot never dominates the view.
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

    // Where an item of the given size scales from: the edge of the frame that
    // faces the dock, so the popover grows out of it instead of its centre.
    function scaleOriginFor(width, height) {
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

    // Natural size of the content in a slot: the window's picture fits the slot
    // without ever stretching it.
    function naturalContentSize(slotRect, sourceWidth, sourceHeight) {
        if (!slotRect || !(slotRect.width > 0) || !(slotRect.height > 0)
                || !(sourceWidth > 0) || !(sourceHeight > 0))
            return Qt.size(0, 0);

        const scale = Math.min(slotRect.width / sourceWidth,
                               slotRect.height / sourceHeight);
        return Qt.size(Math.max(0, sourceWidth * scale - 4),
                       Math.max(0, sourceHeight * scale - 4));
    }

    // Where every window sits in the content, in content coordinates. Both the
    // natural size and the slot targets read this one rule.
    function slotRects(wrappers) {
        const rects = [ ];
        let offset = 0;
        for (let i = 0; i < wrappers.length; ++i) {
            const wrapper = wrappers[i];
            if (!wrapper)
                continue;

            const size = root.previewSlotSize(wrapper, root.isHorizontal);
            if (rects.length > 0)
                offset += root.spacing;
            if (root.isHorizontal) {
                rects.push(Qt.rect(offset, 0, size.width, size.height));
                offset += size.width;
            } else {
                rects.push(Qt.rect(0, offset, size.width, size.height));
                offset += size.height;
            }
        }
        return rects;
    }

    // Natural size of a preview layout for a given set of surfaces. Pure, so a
    // switch can know the final frame size before any layer or slot exists.
    function contentSizeFor(wrappers) {
        const rects = root.slotRects(wrappers);
        if (rects.length === 0)
            return Qt.size(0, 0);

        let cross = 0;
        for (let i = 0; i < rects.length; ++i) {
            const rect = rects[i];
            cross = Math.max(cross, root.isHorizontal ? rect.height : rect.width);
        }

        const last = rects[rects.length - 1];
        return root.isHorizontal
                ? Qt.size(last.x + last.width, cross)
                : Qt.size(cross, last.y + last.height);
    }

    readonly property size contentSize: root.contentSizeFor(root.desiredSurfaces)

    // Last content size that had something to measure. While the shell closes
    // with no windows left the frame keeps this size instead of collapsing
    // around the view: the whole shell scales away, nothing gets clipped.
    property size lastContentSize: Qt.size(0, 0)
    onContentSizeChanged: {
        if (contentSize.width > 0 && contentSize.height > 0)
            lastContentSize = contentSize;
    }

    // Natural size of the tooltip the frame is measured from: the one on
    // screen, or the one that just left.
    readonly property size tooltipSize: tooltip.naturalSize

    // Frame size for a preview layout on this output. A view longer than the
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

    // Content size the frame is measured from. A closing shell has no windows
    // to measure any more, so it holds the last size it had.
    readonly property size frameContentSize:
        (!root.isShowing && root.visible && root.lastContentSize.width > 0)
            ? root.lastContentSize
            : root.contentSize

    function frameSizeFor(kind) {
        return kind === DockPreview.Mode.Tooltip
                   ? root.tooltipSize
                   : root.previewFrameSizeFor(root.frameContentSize);
    }

    readonly property size targetFrameSize: root.frameSizeFor(root.mode)

    // Content area the preview settles at, from the content alone.
    readonly property size previewViewport: {
        const frame = root.previewFrameSizeFor(root.frameContentSize);
        return Qt.size(Math.max(0, frame.width - 2 * root.panelHorizontalPadding),
                       Math.max(0, frame.height - 2 * root.panelVerticalPadding
                                        - root.titleRowHeight - root.spacing));
    }

    // The frame is the only animated geometry.
    property real frameWidth: root.targetFrameSize.width
    property real frameHeight: root.targetFrameSize.height

    // Fade of the shell and of the preview content it carries.
    // Tooltip layers fade on their own.
    property real shellFade: 0

    function setShowing(showing) {
        const entersFromNothing = showing && !root.visible;
        if (showing)
            root.visible = true;
        if (entersFromNothing && !root.tooltipLayout) {
            // Snap to the start value, then let the assignment below animate.
            surface.shellScaleAnimated = false;
            surface.shellScale = 0.1;
            surface.shellScaleAnimated = true;
        }
        // The states used to carry these two numbers; the Behaviours act on the
        // assignment, so entry and exit read the same here as everywhere else.
        root.shellFade = showing ? 1 : 0;
        surface.shellScale = showing ? 1 : 0.5;
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

    // Space the preview view reserves for the title, growing from zero.
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

    // Shared with the components that log on behalf of the popover.
    readonly property LoggingCategory loggingCategory: qLcDockPreview

    LoggingCategory {
        id: qLcDockPreview
        name: "treeland.qml.dockpreview"
        defaultLogLevel: LoggingCategory.Info
    }

    /* --- model begin --- */
    // `desiredSurfaces` is the authority. The slot instances only follow it:
    // these helpers edit the list alone, and the change handler syncs them
    // afterwards.
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
        for (const surfaceWrapper of root.desiredSurfaces) {
            if (surfaceWrapper && surfaceWrapper.shellSurface)
                surfaceWrapper.shellSurface.close();
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

    // The view owns the slots; it tells us when the list ran empty so the
    // popover can close itself instead of leaving an empty frame.
    function syncSurfaces() {
        if (!previewView)
            return;
        previewView.sync();
    }

    function show(surfaces, target, pos, direction) {
        console.info(qLcDockPreview, "start show windows: ", surfaces)
        const wasVisible = root.visible;
        const wasOpen = root.isShowing;
        const wasTooltip = root.tooltipLayout;
        root.parent = target.parent;

        // Keep the slots anchored to the left as much as possible, many
        // animations rely on it. Only a `dock preview` <-> `tooltip` switch may
        // anchor them to the other side.
        if (root.isShowing && wasTooltip)
            root.contentPinToLeft = root.pos?.x - pos?.x <= 0;
        else
            root.contentPinToLeft = true

        root.pos = pos;
        root.direction = direction;
        root.target = target;
        root.mode = DockPreview.Mode.Preview;
        if (!wasOpen || wasTooltip)
            previewView.reset();
        // Drop empty entries once, at the only entry that takes an external list.
        const windows = [];
        for (let i = 0; i < surfaces.length; ++i) {
            if (surfaces[i])
                windows.push(surfaces[i]);
        }
        root.desiredSurfaces = windows;
        // An empty list has nothing to reveal; the view already closed it.
        if (windows.length === 0)
            return;
        previewView.resetScroll();
        // A popover that is already on screen switches with a curve; one that is
        // entering from nothing lands on the final layout right away.
        root.setPreviewReveal(1, wasVisible);
        root.setShowing(true);
        root.tooltipSurface = null;
    }

    function showTooltip(surfaceWrapper, target, pos, direction) {
        console.info(qLcDockPreview, "start show tooltip: ", surfaceWrapper, target, pos, direction)
        const wasVisible = root.visible;
        root.parent = target.parent;
        if (root.isShowing && !root.tooltipLayout)
            root.contentPinToLeft = root.pos?.x - pos?.x >= 0;
        else
            root.contentPinToLeft = true

        root.pos = pos;
        root.direction = direction;
        root.target = target;
        // Publish the surface before the mode: tooltipSize reads the live layer,
        // and switching mode first would let the frame target fall back to the
        // previous tooltip for a moment.
        root.tooltipSurface = surfaceWrapper;
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

    // The state anchors stay here on purpose: they tie the surface's
    // dock-facing edge to root's, which is the one piece of the state that
    // spans two items. Everything that only depends on the frame's own size is
    // computed by the surface itself.
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
                target: surface
                anchors.horizontalCenter: root.horizontalCenter
                anchors.bottom: root.bottom
            }
            PropertyChanges {
                target: root
                anchors.horizontalCenterOffset: root.pos.x - root.target?.width / 2 + horizontalCenterOffsetAdjust
                anchors.bottomMargin: -root.pos.y
                restoreEntryValues: false
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
                target: surface
                anchors.verticalCenter: root.verticalCenter
                anchors.left: root.left
            }
            PropertyChanges {
                target: root
                anchors.verticalCenterOffset: root.pos.y - root.target?.height / 2 + verticalCenterOffsetAdjust
                anchors.leftMargin: root.pos.x
                restoreEntryValues: false
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
                target: surface
                anchors.horizontalCenter: root.horizontalCenter
                anchors.top: root.top
            }
            PropertyChanges {
                target: root
                anchors.horizontalCenterOffset: root.pos.x - root.target?.width / 2 + horizontalCenterOffsetAdjust
                anchors.topMargin: root.pos.y
                restoreEntryValues: false
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
                target: surface
                anchors.verticalCenter: root.verticalCenter
                anchors.right: root.right
            }
            PropertyChanges {
                target: root
                anchors.verticalCenterOffset: root.pos.y - root.target?.height / 2 + verticalCenterOffsetAdjust
                anchors.rightMargin: -root.pos.x
                restoreEntryValues: false
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

    // These parts take the controller as a plain QtObject: a typed reference
    // would make the popover files depend on each other's types, which the QML
    // loader rejects.
    DockPreviewSurface {
        id: surface
        parent: root.parent
        preview: root
    }

    // The preview view: the title row and the slots, inside the surface's
    // content host.
    DockPreviewView {
        id: previewView
        parent: surface.contentHost
        preview: root
        hovered: surface.titleHovered
        onEmptied: {
            // Closing keeps the slots until the shell has faded out; running
            // out of windows while hidden just drops them.
            if (root.isShowing && !root.tooltipLayout) {
                previewView.freezePictures();
                root.close();
                return;
            }
            previewView.clear();
        }
    }

    // The tooltip content: the surface on screen and the layers it left.
    DockPreviewTooltip {
        id: tooltip
        preview: root
        host: surface.contentHost
        layerComponent: Component {
            DockPreviewTooltipLayer {
                preview: root
            }
        }
    }
}

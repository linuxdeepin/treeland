// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

import Treeland
import Treeland.Protocols

Item {
    id: root
    required property DesktopEnvironment desktop
    property int aniDuration: 350
    property bool enableAnimation: aniDuration > 0 && WaylandServer.animationConfig.dockPreview
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
    onTooltipSurfaceChanged: tooltipPresent.applySurface(root.tooltipSurface)

    property bool isShowing: false

    // The three are peers: the frame holds whichever one was asked for, and a
    // switch between any two is a hand-over, not a mode change.
    enum Presentation { Tooltip, Thumbnail, List }
    property int requestedPresentation: DockPreview.Presentation.Tooltip
    // How far each presentation is into the frame. The tooltip has no reveal.
    property real thumbnailReveal: 0
    property real listReveal: 0
    // Which side of the frame the preview content stays glued to.
    property bool contentPinToLeft: true
    // A bottom dock keeps the list growing out of the dock edge.
    readonly property bool listPinnedToBottom:
        root.direction === ForeignToplevelManagerInterfaceV3.PreviewDirection.bottom
    property var desiredSurfaces: []
    onDesiredSurfacesChanged: root.syncSurfaces()

    readonly property real titleRowHeight: titleFontMetrics.height
    // Chrome the thumbnail adds above its content; only it carries a header.
    readonly property real titleRowChrome: root.titleRowHeight + root.spacing

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

    readonly property int listContentWidth: PopoverStyle.listContentWidth
    readonly property int listRowHeight: PopoverStyle.listRowHeight
    readonly property int listRowSpacing: PopoverStyle.listRowSpacing
    readonly property int listMaxFrameHeight: PopoverStyle.listMaxFrameHeight
    readonly property int listMaxContentHeight:
        Math.max(0, root.listMaxFrameHeight - 2 * root.panelVerticalPadding)
    // What the list can actually show: the frame cap and the output both bound
    // it, so a short output never hides rows the scroller cannot reach.
    readonly property real listMaxViewportHeight: Math.max(0,
        Math.min(root.listMaxContentHeight,
                 root.outPutSize.height - 2 * root.screenEdgeMargin
                     - 2 * root.panelVerticalPadding))

    function listRowRects(wrappers) {
        const rects = [ ];
        let offset = 0;
        for (let i = 0; i < wrappers.length; ++i) {
            const wrapper = wrappers[i];
            if (!wrapper)
                continue;

            if (rects.length > 0)
                offset += root.listRowSpacing;
            rects.push(Qt.rect(0, offset, root.listContentWidth, root.listRowHeight));
            offset += root.listRowHeight;
        }
        return rects;
    }

    function listRowsHeight(wrappers) {
        const rects = root.listRowRects(wrappers);
        if (rects.length === 0)
            return 0;
        const last = rects[rects.length - 1];
        return last.y + last.height;
    }

    function listContentSizeFor(wrappers) {
        return Qt.size(root.listContentWidth,
                       Math.min(root.listMaxContentHeight,
                                root.listRowsHeight(wrappers)));
    }

    // What each presentation measures itself from.
    readonly property size thumbnailContentSize: {
        const content = root.contentSizeFor(root.desiredSurfaces);
        return content.width > 0 && content.height > 0
                ? content : root.lastThumbnailContentSize;
    }
    property size lastThumbnailContentSize: Qt.size(0, 0)
    onThumbnailContentSizeChanged: {
        if (root.thumbnailContentSize.width > 0)
            root.lastThumbnailContentSize = root.thumbnailContentSize;
    }

    readonly property size listContentSize: {
        const size = root.listContentSizeFor(root.desiredSurfaces);
        return size.height > 0 ? size : root.lastListContentSize;
    }
    property size lastListContentSize: Qt.size(0, 0)
    onListContentSizeChanged: {
        if (root.listContentSize.height > 0)
            root.lastListContentSize = root.listContentSize;
    }

    // Natural size of the tooltip the frame is measured from: the one on
    // screen, or the one that just left.
    readonly property size tooltipSize: tooltipPresent.naturalSize

    function previewFrameSizeFor(contentSize, titleChrome) {
        const maxContentWidth = Math.max(0, root.outPutSize.width
                                            - 2 * root.screenEdgeMargin
                                            - 2 * root.panelHorizontalPadding);
        const maxContentHeight = Math.max(0, root.outPutSize.height
                                             - 2 * root.screenEdgeMargin
                                             - 2 * root.panelVerticalPadding
                                             - titleChrome);
        return Qt.size(Math.min(contentSize.width, maxContentWidth)
                           + 2 * root.panelHorizontalPadding,
                       Math.min(contentSize.height, maxContentHeight)
                           + titleChrome + 2 * root.panelVerticalPadding);
    }

    readonly property size targetFrameSize:
        root.requestedPresentation === DockPreview.Presentation.Tooltip
            ? root.tooltipSize
      : root.requestedPresentation === DockPreview.Presentation.List
            ? root.previewFrameSizeFor(root.listContentSize, 0)
            : root.previewFrameSizeFor(root.thumbnailContentSize,
                                       root.titleRowChrome)

    readonly property size previewViewport: {
        const frame = root.previewFrameSizeFor(root.thumbnailContentSize,
                                               root.titleRowChrome);
        return Qt.size(Math.max(0, frame.width - 2 * root.panelHorizontalPadding),
                       Math.max(0, frame.height - 2 * root.panelVerticalPadding
                                        - root.titleRowChrome));
    }

    // The frame is the only animated geometry.
    property real frameWidth: root.targetFrameSize.width
    property real frameHeight: root.targetFrameSize.height

    // Fade of the shell and of the preview content it carries.
    // Tooltip layers fade on their own.
    property real shellFade: 0

    function setShowing(showing) {
        root.isShowing = showing;
        const entersFromNothing = showing && !root.visible;
        if (showing)
            root.visible = true;
        if (entersFromNothing
                && root.requestedPresentation !== DockPreview.Presentation.Tooltip) {
            // Snap to the start value, then let the assignment below animate.
            surface.shellScaleAnimated = false;
            surface.shellScale = 0.1;
            surface.shellScaleAnimated = true;
        }
        // The states used to carry these two numbers; the Behaviours act on the
        // assignment, so entry and exit read the same here as everywhere else.
        root.shellFade = showing ? 1 : 0;
        surface.shellScale = showing ? 1 : 0.5;
        // Without the fade, nothing would run the completion below.
        if (!showing && !root.enableAnimation)
            root.finishHide();
    }

    function finishHide() {
        if (root.isShowing)
            return;
        root.visible = false;
        root.desiredSurfaces = [ ];
        root.resetPresentations();
        root.tooltipSurface = null;
    }

    // The shell fade also owns the popover's lifetime: it hides the item (and
    // drops the state it kept alive) once the fade out is over.
    Behavior on shellFade {
        enabled: root.enableAnimation
        SequentialAnimation {
            NumberAnimation {
                id: shellFadeAnimation
                duration: root.aniDuration
                easing.type: Easing.OutExpo
            }
            ScriptAction {
                script: root.finishHide()
            }
        }
    }

    NumberAnimation {
        id: thumbnailRevealAnimation
        target: root
        property: "thumbnailReveal"
        duration: root.aniDuration
        easing.type: Easing.OutExpo
        onFinished: root.settlePresentations()
    }

    NumberAnimation {
        id: listRevealAnimation
        target: root
        property: "listReveal"
        duration: root.aniDuration
        easing.type: Easing.OutExpo
        onFinished: root.settlePresentations()
    }

    // Drive both presentations towards the one that was asked for. One that is
    // asked for again while it is still leaving picks up from where it is
    // instead of jumping back to where it started.
    function updateReveals() {
        const animate = root.visible && root.enableAnimation;
        root.setReveal(thumbnailRevealAnimation, "thumbnailReveal",
                       root.requestedPresentation === DockPreview.Presentation.Thumbnail ? 1 : 0,
                       animate);
        root.setReveal(listRevealAnimation, "listReveal",
                       root.requestedPresentation === DockPreview.Presentation.List ? 1 : 0,
                       animate);
        // A reveal that snapped has no animation to report its end.
        if (!animate)
            root.settlePresentations();
    }

    function setReveal(animation, propertyName, value, animate) {
        if (!animate) {
            animation.stop();
            root[propertyName] = value;
            return;
        }
        if (animation.running) {
            // The same target is already animating: leave it alone.
            if (animation.to === value)
                return;
        } else if (root[propertyName] === value) {
            return;
        }
        animation.stop();
        animation.from = root[propertyName];
        animation.to = value;
        animation.start();
    }

    // A presentation that finished leaving is dropped; once every preview one
    // has left, the windows are no longer needed either.
    function settlePresentations() {
        if (!thumbnailPresent || !listPresent)
            return;
        if (root.thumbnailReveal === 0
                && root.requestedPresentation !== DockPreview.Presentation.Thumbnail)
            thumbnailPresent.clear();
        if (root.listReveal === 0
                && root.requestedPresentation !== DockPreview.Presentation.List)
            listPresent.clear();
        if (root.requestedPresentation === DockPreview.Presentation.Tooltip
                && root.thumbnailReveal === 0 && root.listReveal === 0
                && root.desiredSurfaces.length > 0)
            root.desiredSurfaces = [ ];
    }

    function resetPresentations() {
        thumbnailRevealAnimation.stop();
        listRevealAnimation.stop();
        root.thumbnailReveal = 0;
        root.listReveal = 0;
        root.requestedPresentation = DockPreview.Presentation.Tooltip;
        thumbnailPresent.clear();
        listPresent.clear();
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

    // The content rect every presentation fills: the frame minus its panel
    // padding. A presentation that carries a title row insets itself by it, so
    // no other presentation's geometry depends on that row.
    readonly property real contentLeft: root.panelHorizontalPadding
    readonly property real contentTop: root.panelVerticalPadding
    readonly property real contentWidth:
        Math.max(0, root.frameWidth - 2 * root.panelHorizontalPadding)
    readonly property real contentHeight:
        Math.max(0, root.frameHeight - 2 * root.panelVerticalPadding)

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
        root.desktop.activateWindow(surfaceWrapper)
        root.desktop.windowManagement.leaveDockPreview(root.target.shellSurface.surface)
        root.close();
    }

    function previewWindow(surfaceWrapper) {
        if (!surfaceWrapper)
            return;
        console.debug(qLcDockPreview, "start preview window: ", surfaceWrapper)
        root.desktop.workspace.startPreviewing(surfaceWrapper);
    }

    function stopPreviewWindow() {
        console.debug(qLcDockPreview, "stop preview window")
        root.desktop.workspace.stopPreviewing();
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

    // The requested presentation owns the instances; it tells us when the
    // window list ran empty so the popover can close instead of showing an
    // empty frame.
    function syncSurfaces() {
        if (!thumbnailPresent || !listPresent)
            return;

        switch (root.requestedPresentation) {
        case DockPreview.Presentation.Thumbnail:
            thumbnailPresent.sync();
            break;
        case DockPreview.Presentation.List:
            listPresent.sync();
            break;
        default:
            break;
        }
    }

    function handleEmptied(present) {
        // Closing keeps the presentation until the shell has faded out; running
        // out of windows while hidden just drops it.
        if (root.isShowing && root.requestedPresentation !== DockPreview.Presentation.Tooltip) {
            present.freezeForClose();
            root.close();
            return;
        }
        present.clear();
    }

    function show(surfaces, target, pos, direction, isList) {
        console.info(qLcDockPreview, "start show windows: ", surfaces)
        // Drop empty entries once, at the only entry that takes an external list.
        const windows = [];
        for (let i = 0; i < surfaces.length; ++i) {
            if (surfaces[i])
                windows.push(surfaces[i]);
        }
        // A request that resolves to no window has nothing to switch to, so it
        // takes the popover down whatever presentation is on screen.
        if (windows.length === 0) {
            if (root.isShowing)
                root.close();
            return;
        }

        const wasTooltip = root.requestedPresentation === DockPreview.Presentation.Tooltip;
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
        root.requestedPresentation = isList
                ? DockPreview.Presentation.List
                : DockPreview.Presentation.Thumbnail;
        root.desiredSurfaces = windows;
        // A popover that is already on screen switches with a curve; one that is
        // entering from nothing lands on the final layout right away.
        root.updateReveals();
        root.setShowing(true);
        root.tooltipSurface = null;
    }

    function showTooltip(surfaceWrapper, target, pos, direction) {
        console.info(qLcDockPreview, "start show tooltip: ", surfaceWrapper, target, pos, direction)
        root.parent = target.parent;
        if (root.isShowing
                && root.requestedPresentation !== DockPreview.Presentation.Tooltip)
            root.contentPinToLeft = root.pos?.x - pos?.x >= 0;
        else
            root.contentPinToLeft = true

        root.pos = pos;
        root.direction = direction;
        root.target = target;
        // Publish the surface before the presentation: tooltipSize reads the
        // live layer, and switching presentation first would let the frame
        // target fall back to the previous tooltip for a moment.
        root.tooltipSurface = surfaceWrapper;
        root.requestedPresentation = DockPreview.Presentation.Tooltip;
        // Same as show(): a switch animates the presentation away, an entry
        // from nothing starts with it already collapsed.
        root.updateReveals();
        root.setShowing(true);
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

    // The thumbnail presentation: the title row and the slots, inside the
    // surface's content host.
    DockPreviewThumbnailPresent {
        id: thumbnailPresent
        parent: surface.contentHost
        preview: root
        hovered: surface.titleHovered
        onEmptied: root.handleEmptied(thumbnailPresent)
    }

    DockPreviewListPresent {
        id: listPresent
        parent: surface.contentHost
        preview: root
        onEmptied: root.handleEmptied(listPresent)
    }

    // The tooltip presentation: the surface on screen and the layers it left.
    DockPreviewTooltipPresent {
        id: tooltipPresent
        preview: root
        host: surface.contentHost
        layerComponent: Component {
            DockPreviewTooltipLayer {
                preview: root
            }
        }
    }
}

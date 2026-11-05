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
    property alias spacing: listview.spacing

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
    property bool isNewDockPreview: false
    property bool listviewPinToLeft: true
    // The authoritative list of windows the preview shows.
    property var desiredSurfaces: []
    onDesiredSurfacesChanged: filterSurfaceModel.updateModel()
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
        // A tooltip-state list has no viewport (its size follows the model), so
        // it never clamps currentIndex itself.
        const index = Math.min(Math.max(listview.currentIndex, 0), surfaces.length - 1);
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
    // switch can know the final frame size before any layer or delegate exists.
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

    function setShowing(showing) {
        if (showing)
            root.visible = true;
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
    ListModel {
        id: filterSurfaceModel
        property int lastSize: 0
        onCountChanged: {
            Qt.callLater(()=> { lastSize = count })
        }

        // View mirror of root.desiredSurfaces. Called by its change handler; it
        // only keeps the row count and row contents in step with the authority.
        function updateModel() {
            const surfaces = root.desiredSurfaces;
            const newSize = surfaces.length;
            const oldSize = filterSurfaceModel.count;
            root.isNewDockPreview = oldSize === 0 && newSize !== 0;
            for (let i = 0; i < Math.min(newSize, oldSize); i++) {
                filterSurfaceModel.set(i, {"wrapper": surfaces[i]})
            }
            if (newSize > oldSize) {
                for (let index = oldSize; index < newSize; index++) {
                    filterSurfaceModel.append({"wrapper": surfaces[index]});
                }
            }
            if (newSize < oldSize) {
                filterSurfaceModel.remove(newSize, oldSize - newSize)
            }
        }

        function removeSurface(surfaceWrapper) {
            if (!surfaceWrapper)
                return;

            const index = root.desiredSurfaces.indexOf(surfaceWrapper);
            if (index < 0)
                return;

            // Copy instead of splice: the assignment is what triggers updateModel.
            const surfaces = root.desiredSurfaces.slice();
            surfaces.splice(index, 1);
            root.desiredSurfaces = surfaces;
        }

        function activateWindow(surfaceWrapper) {
            console.debug(qLcDockPreview, "activate preview window: ", surfaceWrapper)
            WaylandServer.forceActivateSurface(surfaceWrapper)
            ForeignToplevelManagerInterfaceV3.leaveDockPreview(root.target.shellSurface.surface)
            root.close();
        }

        function previewWindow(surfaceWrapper) {
            console.debug(qLcDockPreview, "start preview window: ", surfaceWrapper)
            WaylandServer.workspace.startPreviewing(surfaceWrapper);
        }

        function stopPreviewWindow() {
            console.debug(qLcDockPreview, "stop preview window")
            WaylandServer.workspace.stopPreviewing();
        }

        function closeAllWindow() {
            console.debug(qLcDockPreview, "close all preview windows")
            const surfaces = root.desiredSurfaces;
            root.desiredSurfaces = [ ];
            for (let surface of surfaces) {
                if (surface && surface.shellSurface)
                    surface.shellSurface.close();
            }
            root.close();
        }

        function closeSpecialWindow(surfaceWrapper) {
            if (!surfaceWrapper)
                return;

            console.debug(qLcDockPreview, "close a special window: ", surfaceWrapper)
            filterSurfaceModel.removeSurface(surfaceWrapper)
            if (surfaceWrapper.shellSurface)
                surfaceWrapper.shellSurface.close()
        }
    }

    function show(surfaces, target, pos, direction) {
        console.info(qLcDockPreview, "start show windows: ", surfaces)
        const wasVisible = root.visible;
        root.parent = target.parent;

        // We want keep listview ancher to left as possible, many animations rely on it
        // Only when show `dock preview` <-> 'tooltip' animation ancher to right
        if (root.isShowing && root.tooltipLayout)
            root.listviewPinToLeft = root.pos?.x - pos?.x <= 0;
        else
            root.listviewPinToLeft = true

        root.pos = pos;
        root.direction = direction;
        root.target = target;
        root.mode = DockPreview.Mode.Preview;
        // Drop empty entries once, at the only entry that takes an external list.
        const windows = [];
        for (let i = 0; i < surfaces.length; ++i) {
            if (surfaces[i])
                windows.push(surfaces[i]);
        }
        root.desiredSurfaces = windows;
        listview.contentX = 0;
        listview.contentY = 0;
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
            root.listviewPinToLeft = root.pos?.x - pos?.x >= 0;
        else
            root.listviewPinToLeft = true

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

    ListView {
        id: listview
        model: filterSurfaceModel
        // Outside the shell group: the preview content owns its own opacity, so
        // it does not multiply into the shell fade.
        parent: contentHost

        property size lastSize: Qt.size(0, 0)
        property int radius: 5
        // Topmost z of the preview items (delegates).
        readonly property int delegateZBase: 10000
        clip: true
        enabled: !root.tooltipLayout
        width: root.previewViewport.width
        height: root.previewViewport.height
        x: root.listviewPinToLeft ? root.contentLeft
                                  : root.contentLeft + root.contentWidth - width
        y: root.contentTop + root.contentHeight - height
        transformOrigin: root.listviewPinToLeft ? Item.BottomLeft : Item.BottomRight
        scale: {
            if (!(width > 0) || !(height > 0))
                return 0;
            return Math.max(0, Math.min(1, root.contentWidth / width,
                                         root.contentHeight / height));
        }
        // The reveal covers mode switches; the shell fade covers entering and
        // leaving the popover.
        opacity: root.previewReveal * root.shellFade

        orientation: root.isHorizontal ? ListView.Horizontal : ListView.Vertical
        layoutDirection: Qt.LeftToRight
        verticalLayoutDirection: ListView.TopToBottom
        boundsBehavior: Flickable.StopAtBounds
        interactive: true
        highlightFollowsCurrentItem: true
        highlightMoveDuration: root.aniDuration/2
        spacing: 5
        highlight: Control {
            objectName: "highlight"
            id: hoverBorder
            visible: false
            // purely visual overlay: keep it out of hover competition so that
            // the preview item underneath still gets its own hover
            hoverEnabled: false
            // keep the hover border above every preview
            z: listview.delegateZBase + 1
            anchors {
                verticalCenter: listview.orientation === ListView.Horizontal && parent ? parent.verticalCenter : undefined
                horizontalCenter: listview.orientation === ListView.Vertical && parent ? parent.horizontalCenter : undefined
            }

            contentItem: Rectangle {
                anchors.fill: parent
                color: "transparent";
                radius: listview.radius + border.width
                property D.Palette borderPalette: D.Palette {
                    normal {
                        common: Qt.rgba(0, 0, 0, 0.2)
                    }
                    normalDark {
                        common: Qt.rgba(1, 1, 1, 0.3)
                    }
                }

                border.color: D.ColorSelector.borderPalette
                border.width: 4
            }
        }
        focus: true
        delegate: Rectangle {
            id: delegate
            objectName: "delegate"
            // ensure removing items are below non-removing ones
            z: (isRemoving ? -index : (listview.delegateZBase - index))
            visible: true
            radius: listview.radius
            color: PopoverStyle.itemBackground

            required property var index
            required property var wrapper // from filterSurfaceModel
            property bool isRemoving: false
            readonly property size naturalSize: root.previewSlotSize(wrapper, root.isHorizontal)

            implicitWidth: naturalSize.width
            implicitHeight: naturalSize.height

            Connections {
                target: wrapper
                function onAboutToBeInvalidated() {
                    listview.model.removeSurface(wrapper)
                }
            }

            ListView.onRemove: {
                if (filterSurfaceModel.count) {
                    isRemoving = true
                    removeAnimation.start()
                }
            }

            SequentialAnimation {
                id: removeAnimation
                PropertyAction { target: delegate; property: "ListView.delayRemove"; value: true }
                NumberAnimation {
                    target: delegate
                    property: "x"
                    to: {
                        // The last slot is not instantiated once it is outside the
                        // viewport, and `undefined + undefined` is NaN: keep the
                        // item where it is instead of animating to nowhere.
                        const item = listview.itemAtIndex(listview.model.count - 1)
                        if (!item)
                            return delegate.x;
                        return item.x + item.implicitWidth - delegate.implicitWidth
                    }
                    duration: root.aniDuration
                    easing.type: Easing.OutExpo
                }
                PropertyAction { target: delegate; property: "ListView.delayRemove"; value: false }
            }

            HoverHandler {
                id: hoverHandler
                onHoveredChanged: {
                    if (hovered) {
                        listview.model.previewWindow(wrapper)
                        listview.currentIndex = index
                        if (listview.highlightItem)
                            listview.highlightItem.visible = true
                    }
                }
                cursorShape: Qt.ArrowCursor
            }

            TapHandler {
                onTapped: {
                    listview.model.activateWindow(wrapper)
                }
            }

            ShaderEffectSource {
                id: effect
                anchors.centerIn: parent
                implicitHeight: Math.min(parent.implicitHeight, wrapper.height * parent.implicitWidth / wrapper.width) - 4
                implicitWidth: Math.min(parent.implicitWidth, wrapper.width * parent.implicitHeight / wrapper.height) - 4
                live: true
                hideSource: false
                smooth: true
                sourceItem: wrapper
            }

            // one close button per preview, shown while hovering this item
            PopoverCloseButton {
                id: closeButton
                anchors {
                    top: parent.top
                    right: parent.right
                    topMargin: 10
                    rightMargin: 10
                }
                visible: hoverHandler.hovered && !isRemoving
                onClicked: {
                    listview.highlightItem.visible = false
                    // capture the wrapper now: the deferred callback must not
                    // read the delegate's row again, the rows may have been
                    // removed or refreshed by then
                    const target = wrapper
                    Qt.callLater(()=>listview.model.closeSpecialWindow(target))
                }
            }
        }

        add: Transition {
            id: addTransition
            enabled: !root.isNewDockPreview

            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0.3
                    to: 1
                    duration: root.aniDuration
                    easing.type: Easing.OutExpo
                }

                NumberAnimation {
                    property: "x"
                    from: {
                        // The slot the new one slides out from may be outside the
                        // viewport, hence not instantiated: start from the item's
                        // own place instead of animating from NaN.
                        const item = listview.itemAtIndex(listview.model.lastSize - 1)
                        const added = addTransition.ViewTransition.item
                        if (!item)
                            return added ? added.x : 0;
                        return item.x + item.implicitWidth - (added ? added.implicitWidth : 0)
                    }
                    duration: root.aniDuration
                    easing.type: Easing.OutExpo
                }
            }
        }

        HoverHandler {
            onHoveredChanged: {
                if (!hovered) {
                    if (listview.highlightItem)
                        listview.highlightItem.visible = false
                    listview.model.stopPreviewWindow()
                }
            }
            cursorShape: Qt.ArrowCursor
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
            enabled: listview.count !== 0
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
                    listview.model.closeAllWindow()
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
                NumberAnimation {
                    id: scaleAnimation
                    duration: root.aniDuration
                    easing.type: Easing.OutExpo
                }
            }
        }
    }
}

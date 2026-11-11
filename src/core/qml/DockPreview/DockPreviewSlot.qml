// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

import Treeland
import org.deepin.dtk 1.0 as D

// One window in the preview view. The slot owns its own rectangle, the size
// its pictures are drawn at, and everything that fades them.
Item {
    id: slot

    // The DockPreview controller: model edits, geometry helpers and durations.
    required property QtObject preview
    // The preview view this slot belongs to: hover, order and the shared radius.
    required property QtObject view

    property var wrapper: null
    // A var keeps a window after the compositor invalidates it, so the
    // slot tracks liveness itself instead of testing `wrapper`.
    property bool wrapperAlive: false
    property rect fromRect: Qt.rect(0, 0, 0, 0)
    property rect toRect: Qt.rect(0, 0, 0, 0)
    property real progress: 1
    // Whole-slot opacity: a new slot starts at the tail of the stack, fades
    // in as it slides out, and fades out while it collapses.
    property real reveal: 1
    // The slot owns its border; the view only decides which slot shows it.
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
    // Set when the shell closes with no windows left: the slot keeps
    // its last picture while the whole shell fades away.
    property bool frozen: false
    property int slotIndex: -1
    // True while the pointer is over this slot's box, so the view can
    // pick the survivor after a move between slots.
    readonly property bool hovered: slotHover.hovered

    Behavior on contentSize {
        PropertyAnimation {
            duration: slot.contentSizeAnimated ? preview.aniDuration : 0
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
        contentSizeAnimated = animated && preview.enableAnimation;
        contentSize = preview.naturalContentSize(rect,
                                                 newWrapper ? newWrapper.width : 0,
                                                 newWrapper ? newWrapper.height : 0);
    }

    // The only place that writes the geometry. The new animation always
    // starts from the rect the slot is drawn at, not from where the old
    // animation was heading.
    function startMove(rect) {
        const current = currentRect();
        if (!preview.enableAnimation) {
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
        wrapperAlive = true;
        frozen = false;
        setContentSize(newWrapper, rect, false);
        contentOpacity = 1;
        clearOutgoing();
        if (instant || !preview.enableAnimation) {
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

    // A closing slot keeps its own picture. It slides into the rect it is
    // given, starting from where it is drawn, and only disappears
    // once that is done. The picture is the one thing that goes early;
    // the slot and every layer around it stay on the normal duration.
    function collapseTo(rect) {
        collapsing = true;
        // Freeze the picture while the window can still provide it;
        closeSnapshot.scheduleUpdate();
        borderOpacity = 0;
        revealAnimation.stop();
        if (preview.enableAnimation) {
            contentReveal.stop();
            closeContentFade.from = contentOpacity;
            closeContentFade.to = 0;
            closeContentFade.restart();
        }
        startMove(rect);
        if (!preview.enableAnimation) {
            view.dropSlot(slot);
            return false;
        }
        revealAnimation.from = reveal;
        revealAnimation.to = 0;
        revealAnimation.restart();
        return true;
    }

    // The shell closed with no windows left, so nothing takes this
    // slot over. Freeze the picture and let the shell fade take it.
    function freezePicture() {
        if (frozen || collapsing)
            return;
        frozen = true;
        closeSnapshot.scheduleUpdate();
    }

    // The list took this slot back before it was gone: turn around from
    // where it is drawn right now.
    function revive(newWrapper, rect) {
        collapsing = false;
        frozen = false;
        closeContentFade.stop();
        if (newWrapper !== wrapper) {
            swapContent(newWrapper, rect);
        } else {
            setContentSize(newWrapper, rect, true);
            // The picture may have started to disappear.
            if (contentOpacity < 1) {
                if (!preview.enableAnimation) {
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
        if (!preview.enableAnimation) {
            reveal = 1;
            return;
        }
        revealAnimation.from = reveal;
        revealAnimation.to = 1;
        revealAnimation.restart();
    }

    // Move the picture on screen to a layer of its own.
    function demoteContent() {
        if (!wrapper || !wrapperAlive)
            return;

        const layer = outgoingLayerComponent.createObject(contentClip);
        if (!layer) {
            console.warn(preview.loggingCategory, "failed to create a slot layer");
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
        wrapperAlive = true;
        frozen = false;
        setContentSize(newWrapper, slotRect, true);

        contentReveal.stop();
        if (!preview.enableAnimation) {
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
        radius: view.radius
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
            visible: slot.wrapperAlive
            live: true
            hideSource: false
            smooth: true
            sourceItem: slot.wrapper
        }

        // The window may be destroyed before the collapse or the shell fade
        // ends, so the slot shows a frame frozen while the window still had
        // one.
        ShaderEffectSource {
            id: closeSnapshot
            anchors.centerIn: parent
            implicitWidth: slot.contentSize.width
            implicitHeight: slot.contentSize.height
            // The same early fade as the live picture, so a window that
            // is already gone does not keep it on screen.
            opacity: slot.contentOpacity
            visible: slot.collapsing || slot.frozen
            live: false
            hideSource: false
            smooth: true
            z: 1
            sourceItem: (slot.collapsing || slot.frozen) ? contentSource : null
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
                if (!preview.enableAnimation) {
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
                duration: preview.aniDuration
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
        radius: view.radius + 4
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
            slot.wrapperAlive = false;
            preview.removeSurface(slot.wrapper);
        }
    }

    HoverHandler {
        id: slotHover
        enabled: !slot.collapsing && !preview.tooltipLayout
        cursorShape: Qt.ArrowCursor
        onHoveredChanged: {
            if (!hovered)
                return;
            preview.previewWindow(slot.wrapper);
            view.currentSlot = slot.slotIndex;
            view.hoveredSlot = slot;
            view.updateHoverBorder();
        }
    }

    TapHandler {
        onTapped: preview.activateWindow(slot.wrapper)
    }

    // One close button per slot, shown while hovering it.
    DockPreviewCloseButton {
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
            Qt.callLater(()=>preview.closeSpecialWindow(target));
        }
    }

    NumberAnimation {
        id: moveAnimation
        target: slot
        property: "progress"
        from: 0
        to: 1
        duration: preview.aniDuration
        easing.type: Easing.OutExpo
    }

    Behavior on borderOpacity {
        enabled: preview.enableAnimation
        NumberAnimation {
            duration: preview.aniDuration
            easing.type: Easing.OutExpo
        }
    }

    NumberAnimation {
        id: revealAnimation
        target: slot
        property: "reveal"
        duration: preview.aniDuration
        easing.type: Easing.OutExpo
        onFinished: {
            if (slot.collapsing)
                view.dropSlot(slot);
        }
    }

    NumberAnimation {
        id: contentReveal
        target: slot
        property: "contentOpacity"
        duration: preview.aniDuration
        easing.type: Easing.OutExpo
    }

    // Only the closing window's picture goes early. Every layer around
    // it keeps using aniDuration.
    NumberAnimation {
        id: closeContentFade
        target: slot
        property: "contentOpacity"
        duration: preview.closeContentDuration
        easing.type: Easing.OutCubic
    }
}

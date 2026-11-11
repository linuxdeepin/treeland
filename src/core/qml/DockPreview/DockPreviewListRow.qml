// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

import Waylib.Server

Item {
    id: row

    required property QtObject preview
    required property QtObject layerItem

    property var wrapper: null
    // Where the row is drawn. Only `y` moves; the size and the left edge come
    // from the style and the list.
    property real yFrom: 0
    property real yTo: 0
    property real progress: 1
    property real reveal: 1
    property bool dropping: false
    property int rowIndex: -1

    readonly property bool hovered: rowHover.hovered
    // A row takes input only while its list is live and it is not on its way
    // out.
    readonly property bool interactive: layerItem.interactive && !row.dropping
    readonly property int sidePadding: PopoverStyle.listRowPadding
    readonly property int iconSize: PopoverStyle.listIconSize
    readonly property int innerSpacing: PopoverStyle.listInnerSpacing

    objectName: "previewListRow"
    x: 0
    y: row.yFrom + (row.yTo - row.yFrom) * row.progress
    width: preview.listContentWidth
    height: preview.listRowHeight
    opacity: reveal

    // The only place that writes the position. The new animation always starts
    // from where the row is drawn.
    function startMove(top) {
        const current = y;
        if (!preview.enableAnimation) {
            yFrom = top;
            yTo = top;
            progress = 1;
            return;
        }
        yFrom = current;
        yTo = top;
        progress = 0;
        moveAnimation.restart();
    }

    function enter(newWrapper, startTop, top, instant) {
        wrapper = newWrapper;
        if (instant || !preview.enableAnimation) {
            yFrom = top;
            yTo = top;
            progress = 1;
            reveal = 1;
            return;
        }
        yFrom = startTop;
        yTo = top;
        progress = 0;
        moveAnimation.restart();
        reveal = 0;
        revealAnimation.stop();
        revealAnimation.from = 0;
        revealAnimation.to = 1;
        revealAnimation.restart();
    }

    function retarget(newWrapper, top) {
        wrapper = newWrapper;
        // Already heading there: leave the running animation alone.
        if (top === yTo)
            return;
        startMove(top);
    }

    // Dropped from the list: close into the row that takes its place while it
    // fades. False means it left at once and the list must not track it any
    // more.
    function collapseTo(top) {
        dropping = true;
        if (!preview.enableAnimation) {
            layerItem.dropRow(row);
            return false;
        }
        startMove(top);
        revealAnimation.stop();
        revealAnimation.from = reveal;
        revealAnimation.to = 0;
        revealAnimation.restart();
        return true;
    }

    function freezePosition() {
        moveAnimation.stop();
        revealAnimation.stop();
    }

    Rectangle {
        anchors.fill: parent
        radius: PopoverStyle.radius
        color: PopoverStyle.itemBackground
        visible: row.hovered
    }

    Loader {
        id: iconLoader
        readonly property var iconBuffer: row.wrapper?.appIcon ?? null
        x: row.sidePadding
        width: row.iconSize
        height: row.iconSize
        anchors.verticalCenter: parent.verticalCenter
        active: !!iconBuffer
        visible: active
        sourceComponent: Component {
            BufferItem {
                id: iconItem
                width: row.iconSize
                height: row.iconSize
                smooth: true

                Binding {
                    target: iconItem
                    property: "buffer"
                    value: iconLoader.iconBuffer
                    when: !!iconLoader.iconBuffer
                    restoreMode: Binding.RestoreNone
                }
            }
        }
    }

    Text {
        id: titleText
        anchors.left: iconLoader.right
        anchors.leftMargin: row.innerSpacing
        anchors.right: row.hovered ? closeButton.left : parent.right
        anchors.rightMargin: row.hovered ? row.innerSpacing : row.sidePadding
        anchors.verticalCenter: parent.verticalCenter
        text: row.wrapper && row.wrapper.shellSurface
              ? row.wrapper.shellSurface.title : ""
        font: PopoverStyle.titleFont
        color: PopoverStyle.titleColor
        horizontalAlignment: Text.AlignLeft
        elide: Text.ElideRight
    }

    DockPreviewCloseButton {
        id: closeButton
        anchors.right: parent.right
        anchors.rightMargin: row.sidePadding
        anchors.verticalCenter: parent.verticalCenter
        maximumSize: PopoverStyle.listRowButtonSize
        visible: row.hovered
        onClicked: {
            // Capture the wrapper now: the deferred callback must not read the
            // row's content again, it may have been reused.
            const target = row.wrapper;
            Qt.callLater(() => preview.closeSpecialWindow(target));
        }
    }

    HoverHandler {
        id: rowHover
        enabled: row.interactive
        cursorShape: Qt.ArrowCursor
        onHoveredChanged: {
            if (hovered && row.wrapper)
                preview.previewWindow(row.wrapper);
        }
    }

    TapHandler {
        enabled: row.interactive
        onTapped: {
            if (row.wrapper)
                preview.activateWindow(row.wrapper);
        }
    }

    // The window went away on its own: the list has to drop it, exactly like a
    // closed slot asks the controller to.
    Connections {
        target: row.wrapper
        function onAboutToBeInvalidated() {
            preview.removeSurface(row.wrapper);
        }
    }

    NumberAnimation {
        id: moveAnimation
        target: row
        property: "progress"
        from: 0
        to: 1
        duration: preview.aniDuration
        easing.type: Easing.OutExpo
    }

    NumberAnimation {
        id: revealAnimation
        target: row
        property: "reveal"
        duration: preview.aniDuration
        easing.type: Easing.OutExpo
        onFinished: {
            if (row.dropping)
                layerItem.dropRow(row);
        }
    }
}

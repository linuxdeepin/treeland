// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

import Treeland
import org.deepin.dtk 1.0 as D

Item {
    id: root

    required property Flickable view
    required property bool horizontal
    property int duration: 350
    property bool enableAnimation: true
    property bool interactive: true

    readonly property real edgeTolerance: 0.5
    readonly property bool moreAtStart: {
        const position = root.horizontal ? root.view.contentX : root.view.contentY;
        return position > root.edgeTolerance;
    }
    readonly property bool moreAtEnd: {
        const position = root.horizontal ? root.view.contentX : root.view.contentY;
        const size = root.horizontal ? root.view.width : root.view.height;
        const content = root.horizontal ? root.view.contentWidth
                                        : root.view.contentHeight;
        return position + size < content - root.edgeTolerance;
    }

    x: root.view.x
    y: root.view.y
    width: root.view.width
    height: root.view.height
    scale: root.view.scale
    transformOrigin: root.view.transformOrigin
    z: 1

    function stopPage() {
        scrollAnimation.stop();
        const current = root.horizontal ? root.view.contentX : root.view.contentY;
        scrollAnimation.from = current;
        scrollAnimation.to = current;
    }

    // One click is half a viewport, never more than what is left to show.
    function page(direction) {
        const axis = root.horizontal ? "contentX" : "contentY";
        const size = root.horizontal ? root.view.width : root.view.height;
        const content = root.horizontal ? root.view.contentWidth
                                        : root.view.contentHeight;
        const current = root.view[axis];
        const limit = Math.max(0, content - size);
        const target = Math.max(0, Math.min(limit, current + direction * size / 2));
        if (target === current)
            return;

        scrollAnimation.stop();
        if (!root.enableAnimation) {
            root.view[axis] = target;
            return;
        }
        scrollAnimation.property = axis;
        scrollAnimation.from = current;
        scrollAnimation.to = target;
        scrollAnimation.restart();
    }

    component ScrollButton: D.ToolButton {
        id: button

        required property bool horizontal
        property string iconName

        width: button.horizontal ? PopoverStyle.scrollHintThickness
                                 : PopoverStyle.scrollHintLength
        height: button.horizontal ? PopoverStyle.scrollHintLength
                                  : PopoverStyle.scrollHintThickness
        padding: 0
        icon.name: button.iconName
        icon.width: PopoverStyle.scrollHintSize
        icon.height: PopoverStyle.scrollHintSize
        enabled: root.interactive
        opacity: 1
    }

    ScrollButton {
        horizontal: root.horizontal
        iconName: root.horizontal ? "arrow_ordinary_left" : "arrow_ordinary_up"
        x: root.horizontal ? PopoverStyle.scrollHintMargin
                           : Math.round((root.width - width) / 2)
        y: root.horizontal ? Math.round((root.height - height) / 2)
                           : PopoverStyle.scrollHintMargin
        visible: root.moreAtStart
        onClicked: root.page(-1)
    }

    ScrollButton {
        horizontal: root.horizontal
        iconName: root.horizontal ? "arrow_ordinary_right" : "arrow_ordinary_down"
        x: root.horizontal ? root.width - width - PopoverStyle.scrollHintMargin
                           : Math.round((root.width - width) / 2)
        y: root.horizontal ? Math.round((root.height - height) / 2)
                           : root.height - height - PopoverStyle.scrollHintMargin
        visible: root.moreAtEnd
        onClicked: root.page(1)
    }

    // Glide instead of a jump.
    PropertyAnimation {
        id: scrollAnimation
        target: root.view
        duration: root.duration
        easing.type: Easing.OutCubic
    }

    // A hand on the scroller takes over from the glide.
    Connections {
        target: root.view

        function onMovementStarted() {
            root.stopPage();
        }

        function onContentWidthChanged() {
            root.stopPage();
        }

        function onContentHeightChanged() {
            root.stopPage();
        }
    }
}

// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
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
    property alias spacing: listview.spacing
    property bool enableBlur: true
    // set by States
    property bool isHorizontal
    // set by show()
    property var target
    property point pos
    property int direction
    // auto update by binding
    property size outPutSize: Qt.size(target?.output?.width || 9999, target?.output?.height || 9999)
    property point outPutPos: Qt.point(target?.output?.x || 0, target?.output?.y || 0)
    property int horizontalCenterOffsetAdjust: 0
    property int verticalCenterOffsetAdjust: 0
    // set by showTooltip()
    property string tooltip: ""
    // handle by workspace

    property bool isShowing: false
    property bool isTooltip: false
    property bool isNewDockPreview: false
    property bool listviewPinToLeft: true

    visible: false

    LoggingCategory {
        id: qLcDockPreview
        name: "treeland.qml.dockpreview"
        defaultLogLevel: LoggingCategory.Info
    }

    /* --- model begin --- */
    ListModel {
        id: filterSurfaceModel
        property int lastSize: 0
        onCountChanged: {
            Qt.callLater(()=> { lastSize = count })
        }

        property var desiredSurfaces: []
        onDesiredSurfacesChanged:  {
            updateModel();
        }

        function updateModel() {
            var newSize = desiredSurfaces.length;
            var oldSize = filterSurfaceModel.count;
            root.isNewDockPreview = oldSize === 0 && newSize !== 0;
            for (let i = 0; i < Math.min(newSize, oldSize); i++) {
                filterSurfaceModel.set(i, {"wrapper": desiredSurfaces[i]})
            }
            if (newSize > oldSize) {
                for (let index = oldSize; index < newSize; index++) {
                    filterSurfaceModel.append({"wrapper": desiredSurfaces[index]});
                }
            }
            if (newSize < oldSize) {
                filterSurfaceModel.remove(newSize, oldSize - newSize)
            }
        }

        function activateWindow(surfaceWrapper) {
            console.debug(qLcDockPreview, "activate preview window: ", surfaceWrapper)
            Helper.activateSurface(surfaceWrapper)
            ForeignToplevelV1.leaveDockPreview(root.target.shellSurface.surface)
            root.close();
        }

        function previewWindow(surfaceWrapper) {
            console.debug(qLcDockPreview, "start preview window: ", surfaceWrapper)
            Helper.workspace.hideAllSurfacesExceptPreviewing(surfaceWrapper);
        }

        function stopPreviewWindow() {
            console.debug(qLcDockPreview, "stop preview window")
            Helper.workspace.showAllSurfaces();
        }

        function closeAllWindow() {
            console.debug(qLcDockPreview, "close all preview windows")
            var tmp = filterSurfaceModel.desiredSurfaces;
            filterSurfaceModel.desiredSurfaces = [ ];
            for (let surface of tmp) {
                surface.shellSurface.close();
            }
            root.close();
        }

        function closeSpecialWindow(surfaceId) {
            let surfaceWrapper = filterSurfaceModel.get(surfaceId)?.wrapper
            console.debug(qLcDockPreview, "close a special window: ", surfaceWrapper)
            filterSurfaceModel.remove(surfaceId)
            surfaceWrapper.shellSurface.close();
        }
    }

    function show(surfaces, target, pos, direction) {
        console.info(qLcDockPreview, "start show windows: ", surfaces)
        root.parent = target.parent;

        // We want keep listview ancher to left as possible, many animations rely on it
        // Only when show `dock preview` <-> 'tooltip' animation ancher to right
        if (root.isShowing && root.isTooltip)
            root.listviewPinToLeft = root.pos?.x - pos?.x <= 0;
        else
            root.listviewPinToLeft = true
        filterSurfaceModel.desiredSurfaces = surfaces
        root.pos = pos;
        root.direction = direction;
        root.target = target;
        if (root.isShowing && root.isTooltip)
            tooltip2PreViewAnimation.start();
        root.isTooltip = false;
        root.isShowing = true;
    }

    function showTooltip(tooltip, target, pos, direction) {
        console.info(qLcDockPreview, "start show tooltip: ", tooltip, target, pos, direction)
        root.parent = target.parent;
        if (root.isShowing && !root.isTooltip)
            root.listviewPinToLeft = root.pos?.x - pos?.x >= 0;
        else
            root.listviewPinToLeft = true
        root.tooltip = tooltip;
        root.pos = pos;
        root.direction = direction;
        root.target = target;
        if (root.isShowing && !root.isTooltip)
            preView2TooltipAnimation.start();
        root.isTooltip = true;
        root.isShowing = true;
    }

    function close() {
        console.info(qLcDockPreview, "stop dock preview")
        root.isShowing = false;
    }
    /* --- model end --- */

    /* --- global position begin --- */
    implicitWidth: getWidth(false)
    implicitHeight: getHeight(false)

    onWidthChanged: {
        var leftSpace = root.x - outPutPos.x;
        if (leftSpace < 0) {
            horizontalCenterOffsetAdjust = -leftSpace + 10;
            return;
        }
        var rightSpace = outPutPos.x + outPutSize.width - (root.x + root.width);
        if (rightSpace < 0) {
            horizontalCenterOffsetAdjust = rightSpace - 10;
            return;
        }
        horizontalCenterOffsetAdjust = 0
    }
    onHeightChanged: {
        var topSpace = root.y - outPutPos.y;
        if (topSpace < 0) {
            verticalCenterOffsetAdjust = -topSpace;
            return;
        }
        var bottomSpace = outPutPos.y + outPutSize.height - (root.y + root.height);
        if (bottomSpace < 0) {
            verticalCenterOffsetAdjust = bottomSpace;
            return;
        }
        verticalCenterOffsetAdjust = 0
    }

    states: [
        State {
            name: "dock_bottom"
            when: direction === ForeignToplevelV1.PreviewDirection.bottom
            AnchorChanges {
                target: root
                anchors.horizontalCenter: root.target?.horizontalCenter
                anchors.bottom: root.target?.top
            }
            AnchorChanges {
                target: background
                anchors.horizontalCenter: root.horizontalCenter
                anchors.bottom: root.bottom
                // horizontalCenterOffset always follow root.horizontalCenterOffset
                // bottomMargin always set to zero
            }
            PropertyChanges {
                target: root
                anchors.horizontalCenterOffset: root.pos.x - root.target?.width / 2 + horizontalCenterOffsetAdjust
                anchors.bottomMargin: -root.pos.y
                isHorizontal: true
                restoreEntryValues: false
            }
            PropertyChanges {
                target: scaleTransform
                origin.x: background.implicitWidth / 2
                origin.y: background.implicitHeight
            }
        },
        State {
            name: "dock_left"
            when: direction === ForeignToplevelV1.PreviewDirection.left
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
                isHorizontal: false
                restoreEntryValues: false
            }
            PropertyChanges {
                target: scaleTransform
                origin.x: 0
                origin.y: background.implicitHeight / 2
            }
        },
        State {
            name: "dock_top"
            when: direction === ForeignToplevelV1.PreviewDirection.top
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
                isHorizontal: true
                restoreEntryValues: false
            }
            PropertyChanges {
                target: scaleTransform
                origin.x: background.implicitWidth / 2
                origin.y: 0
            }
        },
        State {
            name: "dock_right"
            when: direction === ForeignToplevelV1.PreviewDirection.right
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
                isHorizontal: false
                restoreEntryValues: false
            }
            PropertyChanges {
                target: scaleTransform
                origin.x: background.implicitWidth
                origin.y: background.implicitHeight / 2
            }
        }
    ]

    Behavior on anchors.horizontalCenterOffset {
        enabled: root.visible
        NumberAnimation {
            duration: root.aniDuration
        }
    }

    Behavior on anchors.verticalCenterOffset {
        enabled: root.visible
        NumberAnimation {
            duration: root.aniDuration
        }
    }
    /* --- global position end --- */

    TextMetrics {
        id: tooltipMetrics
        font.pointSize: 12
        text: root.tooltip
    }

    function getWidth(removing) {
        let tooltipWidth = Math.min(tooltipMetrics.width + 10, root.outPutSize.width);
        let width = 0
        var reseverWidth = listview.orientation === ListView.Horizontal ? -listview.spacing : 0

        let onlyRemove = false;

        if (removing && root.isTooltip)
            return tooltipWidth;

        for (let child of listview.contentItem.visibleChildren) {
            if (child.objectName === "highlight" || (removing && child.isRemoving)) continue
            if (listview.orientation === ListView.Horizontal) {
                let tmp = width + (child.implicitWidth + listview.spacing)
                if (tmp > outPutSize.width) {
                    return width + reseverWidth
                } else {
                    width = tmp
                }
            } else {
                width = Math.max(width, child.implicitWidth)
            }
        }

        width += reseverWidth + 2 * listview.spacing

        if (width < tooltipWidth)
            width = tooltipWidth;
        return width
    }

    function getHeight(removing) {
        let height = 0
        let reseverHeight = headLayout.implicitHeight + (listview.orientation === ListView.Vertical ? 0 : listview.spacing)
        if (removing && root.isTooltip)
            return tooltipMetrics.height;
        for (let child of listview.contentItem.visibleChildren) {
            if (child.objectName === "highlight" || (removing && child.isRemoving)) continue
            if (listview.orientation === ListView.Vertical) {
                let tmp = height + (child.implicitHeight + listview.spacing)
                if (tmp > outPutSize.height) {
                    return height + reseverHeight
                } else {
                    height = tmp
                }
            } else {
                height = Math.max(height, child.implicitHeight)
            }
        }
        return height + reseverHeight + listview.spacing
    }

    onListviewPinToLeftChanged: {
        // We must clear all left/right anchors then reset them
        listview.anchors.left = undefined
        listview.anchors.right = undefined
        listview.anchors.left = root.listviewPinToLeft ? listview.parent.left : undefined
        listview.anchors.right = root.listviewPinToLeft ? undefined : listview.parent.right
    }

    ListView {
        id: listview
        model: filterSurfaceModel
        parent: background

        property size lastSize: Qt.size(0, 0)
        property int radius: 5
        clip: false
        anchors.left: parent.left // Maybe changed in `onListviewPinToLeftChanged`
        anchors.leftMargin: listview.spacing
        anchors.rightMargin: listview.spacing
        anchors.bottom: parent.bottom
        transformOrigin: root.listviewPinToLeft ? Item.BottomLeft : Item.BottomRight

        orientation: root.isHorizontal ? ListView.Horizontal : ListView.Vertical
        layoutDirection: Qt.LeftToRight
        verticalLayoutDirection: ListView.TopToBottom
        boundsBehavior: Flickable.StopAtBounds
        interactive: true
        highlightFollowsCurrentItem: true
        implicitHeight: root.implicitHeight - headLayout.implicitHeight
        implicitWidth: root.implicitWidth
        highlightMoveDuration: 200
        spacing: 5
        highlight: Control {
            objectName: "highlight"
            id: hoverBorder
            visible: false
            z: listview.z + 2
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
                D.ToolButton {
                    anchors {
                        top: parent.top
                        right: parent.right
                        topMargin: 10
                        rightMargin: 10
                    }
                    implicitWidth: 24
                    implicitHeight: 24
                    icon.name: "close"
                    icon.width: 16
                    icon.height: 16
                    onClicked: {
                        listview.highlightItem.visible = false
                        Qt.callLater(()=>listview.model.closeSpecialWindow(listview.currentIndex))
                    }
                }
            }
        }
        focus: true
        delegate: Rectangle {
            id: delegate
            objectName: "delegate"
            visible: true
            radius: listview.radius
            color: Qt.rgba(0, 0, 0, 0.05) // TODO: dark mode

            required property var index
            required property var wrapper // from filterSurfaceModel
            property bool isRemoving: false

            implicitHeight: root.isHorizontal ? 120 : Math.max(80, Math.min(120, 240 * wrapper.height / wrapper.width))
            implicitWidth: root.isHorizontal ? Math.max(80, Math.min(240, 120 * wrapper.width / wrapper.height)) : 240

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
                        var item = listview.itemAtIndex(listview.model.count - 1)
                        return item?.x + item?.implicitWidth - delegate.implicitWidth
                    }
                    duration: root.aniDuration
                }
                PropertyAction { target: delegate; property: "ListView.delayRemove"; value: false }
            }

            HoverHandler {
                onHoveredChanged: {
                    if (hovered) {
                        listview.model.previewWindow(wrapper)
                        listview.currentIndex = index
                        if (listview.highlightItem)
                            listview.highlightItem.visible = true
                    }
                }
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
                }

                NumberAnimation {
                    property:"x"
                    from: {
                        var item = listview.itemAtIndex(listview.model.lastSize - 1)
                        return item?.x + item?.implicitWidth - addTransition.ViewTransition.item?.implicitWidth
                    }
                    duration: root.aniDuration
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
        }
    }

    SequentialAnimation {
        id: preView2TooltipAnimation
        ParallelAnimation {
            NumberAnimation {
                target: listview
                property: "scale"
                from: 1
                to: 0
                duration: root.aniDuration
            }
        }
        ScriptAction {
            script: {
                filterSurfaceModel.desiredSurfaces = [ ];
                listview.scale = 1 // restore scale property after animation
            }
        }
    }

    SequentialAnimation {
        id: tooltip2PreViewAnimation
        ParallelAnimation {
            NumberAnimation {
                target: listview
                property: "scale"
                from: 0
                to: 1
                duration: root.aniDuration
            }
        }
        PropertyAction { target: root; property: "tooltip"; value: "" }
    }

    Rectangle {
        id: background
        z: root.z
        color: "transparent"
        implicitWidth: getWidth(true) + 2 * listview.spacing
        implicitHeight: getHeight(true) + 2 * listview.spacing
        radius: listview.radius
        clip: false
        parent: root.parent
        visible: root.visible

        Blur {
            anchors.fill: parent
            visible: root.enableBlur
            radius: listview.radius
            blurEnabled: root.enableBlur
        }

        HoverHandler {
            id: titleHover
            enabled: listview.count !== 0
            onHoveredChanged: {
                if (!hovered) {
                    ForeignToplevelV1.leaveDockPreview(root.target.shellSurface.surface)
                } else {
                    ForeignToplevelV1.enterDockPreview(root.target.shellSurface.surface)
                }
            }
        }

        RowLayout {
            id: headLayout
            spacing: 4

            parent: background
            anchors {
                left: background.left
                right: background.right
                top: background.top
                leftMargin: 4
                rightMargin: 4
                topMargin: 4
            }

            Rectangle {
                id: titleOrTooltipRect
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                Layout.fillHeight: true
                implicitWidth: root.isTooltip ?  tooltipText.implicitWidth : titleIconText.implicitWidth
                implicitHeight: root.isTooltip ?  tooltipText.implicitHeight : titleIconText.implicitHeight
                color: "transparent"

                states: [
                    State {
                        name: "tooltip_visible"
                        when: root.isShowing && root.isTooltip
                        PropertyChanges {
                            restoreEntryValues: false // don't restore visible
                            tooltipText {
                                visible: true
                                scale: 1
                                opacity: 1
                            }
                            titleIconText {
                                scale: 0.2
                                opacity: 0
                            }
                        }
                    },
                    State {
                        name: "title_visible"
                        when: root.isShowing && !root.isTooltip
                        PropertyChanges {
                            restoreEntryValues: false
                            tooltipText {
                                scale: 0.2
                                opacity: 0
                            }
                            titleIconText {
                                visible: true
                                scale: 1
                                opacity: 1
                            }
                        }
                    },
                    State {
                        name: "all_hide"
                        when: !root.isShowing
                        PropertyChanges {
                            restoreEntryValues: false
                            tooltipText {
                                visible: false
                                scale: 1
                                opacity: 1
                            }
                            titleIconText {
                                visible: false
                                scale: 1
                                opacity: 1
                            }
                        }
                    }
                ]

                Text {
                    id: tooltipText
                    anchors.fill: parent
                    text: root.tooltip
                    font.pointSize: 12  // FIXME: D.DTK.fontManager.t6 can't work under waylib qpa
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignLeft
                    leftPadding: Math.min((width - tooltipMetrics.width) / 2, 5)
                    // Here need AlignHCenter, but it will cause the position suddenly change on preview to tooltip animation
                    verticalAlignment: Text.AlignVCenter
                    transformOrigin: Item.BottomLeft
                    Behavior on opacity {
                        enabled: root.isShowing
                        SequentialAnimation {
                            NumberAnimation { duration: root.aniDuration }
                            PropertyAction { target: tooltipText; property: "visible"; value: root.isTooltip }
                        }
                    }
                    Behavior on scale {
                        enabled: root.isShowing
                        NumberAnimation { duration: root.aniDuration }
                    }
                }

                RowLayout {
                    id: titleIconText
                    anchors.fill: parent
                    spacing: 2
                    transformOrigin: Item.BottomLeft
                    Rectangle { // TODO: We can't get app icon now, use Rectangle as fallback!
                        width: 24
                        height: 24
                        color: "yellow"
                    }
                    Text {
                        text: filterSurfaceModel.count ?
                                  filterSurfaceModel.get(Math.max(listview.currentIndex, 0)).wrapper.shellSurface.title : ""
                        font.pointSize: 14  // FIXME: D.DTK.fontManager can't work under waylib qpa
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        Layout.fillWidth: true
                    }

                    Behavior on opacity {
                        enabled: root.isShowing
                        SequentialAnimation {
                            NumberAnimation { duration: root.aniDuration }
                            PropertyAction { target: titleIconText; property: "visible"; value: !root.isTooltip }
                        }
                    }
                    Behavior on scale {
                        enabled: root.isShowing
                        NumberAnimation { duration: root.aniDuration }
                    }
                }
            }

            D.ToolButton {
                implicitWidth: 24
                implicitHeight: 24
                visible: titleHover.hovered
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                Layout.rightMargin: 6
                icon.name: "close"
                icon.width: 16
                icon.height: 16
                onClicked: {
                    listview.model.closeAllWindow()
                }
            }
        }

        Behavior on implicitHeight {
            enabled: background.visible
            NumberAnimation { duration: root.aniDuration }
        }

        Behavior on implicitWidth {
            enabled: background.visible
            NumberAnimation { duration: root.aniDuration }
        }

        Behavior on anchors.horizontalCenterOffset {
            enabled: root.visible
            NumberAnimation {
                duration: root.aniDuration
            }
        }

        Behavior on anchors.verticalCenterOffset {
            enabled: root.visible
            NumberAnimation {
                duration: root.aniDuration
            }
        }

        transform: Scale {
            id: scaleTransform
            xScale: 0.5
            yScale: xScale

            Behavior on xScale {
                NumberAnimation {
                    duration: root.aniDuration
                    easing.type: root.isShowing ? Easing.OutExpo : Easing.InExpo
                }
            }
        }

        Behavior on opacity {
            SequentialAnimation {
                ScriptAction {
                    script: {
                        if (root.isShowing)
                          root.visible = true;
                    }
                }
                NumberAnimation {
                    duration: root.aniDuration
                    easing.type: root.isShowing ? Easing.OutExpo : Easing.InExpo
                }
                ScriptAction {
                    script: {
                        if (!root.isShowing) {
                            root.visible = false;
                            root.tooltip = ""
                            filterSurfaceModel.desiredSurfaces = [ ];
                        }
                    }
                }
            }
        }

        states: [
            State {
                name: "invisible"
                when: !root.isShowing
                PropertyChanges {
                    scaleTransform {
                        xScale: 0.5
                    }

                    background {
                        opacity: 0.0
                    }
                }
            },
            State {
                name: "visible"
                when: root.isShowing
                PropertyChanges {
                    scaleTransform {
                        xScale: 1.0
                    }

                    background {
                        opacity: 1.0
                    }
                }
            }
        ]
    }
}

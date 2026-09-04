// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DamageGraphDemo

ApplicationWindow {
    id: win
    width: 1440
    height: 860
    minimumWidth: 1080
    minimumHeight: 680
    visible: true
    title: "Waylib 损伤跟踪器"
    color: "#0a0f1a"

    readonly property color panelColor: "#111827"
    readonly property color raisedColor: "#172033"
    readonly property color borderColor: "#263247"
    readonly property color textColor: "#f3f6fb"
    readonly property color mutedColor: "#8996aa"
    readonly property color accentColor: "#6d7cff"
    property var sel: scene.selectedProps
    property int damageHistoryDuration: 200
    readonly property int detectedRefreshRate: win.screen && win.screen.refreshRate > 0
                                               ? Math.round(win.screen.refreshRate) : 60
    property int sceneRefreshRate: detectedRefreshRate
    property bool treeDragging: false
    property int treeDragId: 0
    property string treeDragName: ""
    property int treeDropTargetId: 0
    property int treeDropParentId: 0
    property int treeDropBeforeId: 0
    property int treeDropMode: 0
    property real treePressX: 0
    property real treePressY: 0
    property real treeGhostX: 0
    property real treeGhostY: 0
    property string treeDropHint: ""
    property bool demoPausedByInteraction: false

    function beginUserInteraction() {
        if (scene.demoRunning) {
            demoPausedByInteraction = true
            scene.demoRunning = false
        }
    }

    function endUserInteraction() {
        if (demoPausedByInteraction) {
            demoPausedByInteraction = false
            scene.demoRunning = true
        }
    }
    palette.window: color
    palette.windowText: textColor
    palette.base: raisedColor
    palette.alternateBase: panelColor
    palette.text: textColor
    palette.button: raisedColor
    palette.buttonText: textColor
    palette.highlight: accentColor
    palette.highlightedText: "#ffffff"
    palette.mid: borderColor

    DemoScene {
        id: scene
        refreshRate: win.sceneRefreshRate
        Component.onCompleted: loadDemoScene("occlusion")
    }

    Connections {
        target: scene
        function onSceneChanged() {
            if (!win.treeDragging)
                win.resetTreeDrag()
        }
    }

    function num(v, fallback) {
        const n = Number(v)
        return Number.isFinite(n) ? n : (fallback ?? 0)
    }

    function typeLabel(type) {
        if (type === "Geometry")
            return "几何节点"
        if (type === "Backdrop")
            return "背景采样节点"
        if (type === "Transform")
            return "变换节点"
        return "分组节点"
    }

    function revealSelectedNode() {
        for (let i = 0; i < scene.treeNodes.length; ++i) {
            if (scene.treeNodes[i].id === scene.selectedId) {
                tree.positionViewAtIndex(i, ListView.Contain)
                return
            }
        }
    }
    function isNodeDescendantOf(targetId, ancestorId) {
        if (!targetId || !ancestorId)
            return false
        if (targetId === ancestorId)
            return true
        let curr = targetId
        while (curr !== 0) {
            let found = false
            for (let i = 0; i < scene.treeNodes.length; ++i) {
                const item = scene.treeNodes[i]
                if (item.id === curr) {
                    if (item.parentId === ancestorId)
                        return true
                    curr = item.parentId || 0
                    found = true
                    break
                }
            }
            if (!found || curr === 0)
                break
        }
        return false
    }

    function calculateTreeDrop(mouseXInTree, mouseYInTree) {
        const count = scene.treeNodes.length
        if (!count || !treeDragId) {
            treeDropTargetId = 0
            treeDropMode = 0
            treeDropHint = ""
            return
        }

        let index = tree.indexAt(mouseXInTree, mouseYInTree)
        if (index < 0) {
            if (mouseYInTree <= 0)
                index = 0
            else
                index = count - 1
        }
        index = Math.max(0, Math.min(count - 1, index))
        const target = scene.treeNodes[index]
        if (!target) {
            treeDropTargetId = 0
            treeDropMode = 0
            treeDropHint = ""
            return
        }

        if (target.id === treeDragId) {
            treeDropTargetId = 0
            treeDropMode = 0
            treeDropHint = "保持原位置"
            return
        }
        if (isNodeDescendantOf(target.id, treeDragId)) {
            treeDropTargetId = 0
            treeDropMode = 0
            treeDropHint = "无法移动至子孙节点"
            return
        }

        // Each delegate has height 36 + spacing 2 = 38
        const rowTop = index * 38 - tree.contentY
        const relY = mouseYInTree - rowTop

        let dragParentId = 0
        let dragIndex = -1
        for (let i = 0; i < count; ++i) {
            if (scene.treeNodes[i].id === treeDragId) {
                dragParentId = scene.treeNodes[i].parentId || 0
                dragIndex = i
                break
            }
        }

        if (target.id === scene.treeNodes[0].id) {
            treeDropTargetId = target.id
            treeDropMode = 2
            treeDropParentId = target.id
            treeDropBeforeId = 0
            treeDropHint = "作为 " + target.name + " 的子节点"
            return
        }

        if (relY < 10) {
            const targetParentId = target.parentId || 0
            if (targetParentId === dragParentId && dragIndex >= 0 && index === dragIndex + 1) {
                treeDropTargetId = 0
                treeDropMode = 0
                treeDropHint = "保持原位置"
                return
            }
            treeDropTargetId = target.id
            treeDropMode = 1
            treeDropParentId = targetParentId
            treeDropBeforeId = target.id
            treeDropHint = "插入到 " + target.name + " 之前"
        } else if (relY > 26) {
            const targetParentId = target.parentId || 0
            if (targetParentId === dragParentId && dragIndex >= 0 && index === dragIndex - 1) {
                treeDropTargetId = 0
                treeDropMode = 0
                treeDropHint = "保持原位置"
                return
            }
            treeDropTargetId = target.id
            treeDropMode = 3
            treeDropParentId = targetParentId
            const next = scene.treeNodes[index + 1]
            if (next && (next.parentId || 0) === targetParentId)
                treeDropBeforeId = next.id
            else
                treeDropBeforeId = 0
            treeDropHint = "插入到 " + target.name + " 之后"
        } else {
            treeDropTargetId = target.id
            treeDropMode = 2
            treeDropParentId = target.id
            treeDropBeforeId = 0
            treeDropHint = "作为 " + target.name + " 的子节点"
        }
    }
    function resetTreeDrag() {
        treeDragging = false
        treeDragId = 0
        treeDropTargetId = 0
        treeDropParentId = 0
        treeDropBeforeId = 0
        treeDropMode = 0
        treeDropHint = ""
        treeDragName = ""
    }
    Action {
        id: addGeometryAction
        text: "几何节点"
        shortcut: "Ctrl+Shift+G"
        onTriggered: scene.addGeometry()
    }
    Action {
        id: addTransformAction
        text: "变换节点"
        shortcut: "Ctrl+Shift+T"
        onTriggered: scene.addTransform()
    }
    Action {
        id: addClipAction
        text: "裁剪节点"
        onTriggered: scene.addClip()
    }
    Action {
        id: addBackdropAction
        text: "背景采样节点"
        onTriggered: scene.addBackdrop()
    }
    Action {
        id: addRendererAction
        text: "自定义渲染节点"
        onTriggered: scene.addRenderer()
    }
    Action {
        id: addGroupAction
        text: "分组节点"
        onTriggered: scene.addBasic()
        shortcut: "Ctrl+Shift+N"
    }
    Action {
        id: moveUpAction
        text: "上移一层"
        enabled: !!win.sel.canRaise
        onTriggered: scene.raiseSelected()
    }
    Action {
        id: moveDownAction
        text: "下移一层"
        enabled: !!win.sel.canLower
        onTriggered: scene.lowerSelected()
    }
    Action {
        id: visibilityAction
        text: win.sel.visible ? "隐藏节点" : "显示节点"
        enabled: !!win.sel.id
        onTriggered: scene.setVisibleSelected(!win.sel.visible)
    }
    Action {
        id: dirtyAction
        text: "标记内容损伤"
        shortcut: "Ctrl+D"
        onTriggered: scene.markSelectedContentDirty()
    }
    Action {
        id: deleteAction
        text: "删除节点"
        shortcut: "Delete"
        onTriggered: scene.removeSelected()
    }

    Menu {
        id: addNodeMenu
        y: addNodeButton.height + 6
        MenuItem { action: addGeometryAction }
        MenuItem { action: addTransformAction }
        MenuItem { action: addClipAction }
        MenuItem { action: addBackdropAction }
        MenuItem { action: addRendererAction }
        MenuSeparator {}
        MenuItem { action: addGroupAction }
    }

    Menu {
        id: nodeMenu
        title: win.sel.name || "节点"
        Menu {
            title: "新增子节点"
            MenuItem { action: addGeometryAction }
            MenuItem { action: addTransformAction }
            MenuItem { action: addClipAction }
            MenuItem { action: addBackdropAction }
            MenuItem { action: addRendererAction }
            MenuItem { action: addGroupAction }
        }
        MenuSeparator {}
        MenuItem { action: moveUpAction }
        MenuItem { action: moveDownAction }
        MenuItem { action: visibilityAction }
        MenuItem { action: dirtyAction }
        MenuSeparator {}
        MenuItem { action: deleteAction }
    }

    Menu {
        id: treeOptionsMenu
        y: treeOptionsButton.height + 6
        MenuItem {
            text: "选择根节点"
            onTriggered: scene.selectedId = scene.treeNodes.length ? scene.treeNodes[0].id : 0
        }
        MenuItem {
            text: "立即提交"
            onTriggered: scene.commit()
        }
        MenuSeparator {}
        MenuItem {
            text: "清空节点树"
            onTriggered: scene.clearTree()
    }
    }

    header: Rectangle {
        height: 72
        color: "#0f1625"
        border.color: win.borderColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 18
            spacing: 18

            ColumnLayout {
                spacing: 1
                Layout.minimumWidth: 250
                Label {
                    text: "损伤跟踪器"
                    color: win.textColor
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                Label {
                    text: "场景树、遮挡剔除与多输出损伤"
                    color: win.mutedColor
                    font.pixelSize: 11
                }
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: 10
                Label { text: "图例"; color: win.mutedColor; font.pixelSize: 10 }
                Rectangle {
                    width: 40; height: 16; radius: 4
                    border.color: "#8794ff"
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#a6b0ff" }
                        GradientStop { position: 1; color: "#4e5ee8" }
                    }
                }
                Label { text: "节点"; color: win.mutedColor; font.pixelSize: 10 }
                Rectangle { width: 40; height: 16; radius: 3; color: "#2dc470" }
                Label { text: "较旧损伤"; color: win.mutedColor; font.pixelSize: 10 }
                Rectangle { width: 40; height: 16; radius: 3; color: "#e83e4e" }
                Label { text: "最新损伤"; color: win.mutedColor; font.pixelSize: 10 }

                Rectangle { width: 1; height: 34; color: win.borderColor }

                ComboBox {
                    id: demoSceneCombo
                    Layout.preferredWidth: 150
                    model: scene.demoScenes
                    textRole: "text"
                    valueRole: "value"
                    onActivated: scene.loadDemoScene(currentValue)
                    ToolTip.visible: hovered
                    ToolTip.text: "选择只读自动演示场景"
                }
                Button {
                    text: scene.demoRunning ? "暂停演示" : "继续演示"
                    onClicked: scene.demoRunning = !scene.demoRunning
                }
                Label {
                    text: scene.demoRunning ? "自动演示中" : "演示已暂停"
                    color: scene.demoRunning ? "#8dc8e8" : "#ffcf70"
                    font.pixelSize: 10
                }
                Switch {
                    text: "自动提交"
                    checked: scene.autoCommit
                    onToggled: scene.autoCommit = checked
                    ToolTip.visible: hovered
                    ToolTip.text: "每次编辑后自动提交"
                }
                Button {
                    text: "提交"
                    highlighted: true
                    onClicked: scene.commit()
                }
            }
    }

        }
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Rectangle {
            SplitView.preferredWidth: 330
            SplitView.minimumWidth: 285
            color: win.panelColor
            border.color: win.borderColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Label {
                            text: "节点树"
                            color: win.mutedColor
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.2
                        }
                        Label {
                            text: "共 " + scene.treeNodes.length + " 个节点"
                            color: win.textColor
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                    }
                    ToolButton {
                        id: treeOptionsButton
                        text: "•••"
                        onClicked: treeOptionsMenu.popup(treeOptionsButton, 0,
                                                         treeOptionsButton.height + 6)
                        ToolTip.visible: hovered
                        ToolTip.text: "节点树菜单"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        id: addNodeButton
                        Layout.fillWidth: true
                        text: "＋  新增子节点"
                        onClicked: addNodeMenu.popup(addNodeButton, 0,
                                                    addNodeButton.height + 6)
                    }
                    ToolButton {
                        text: "↑"
                        enabled: moveUpAction.enabled
                        onClicked: moveUpAction.trigger()
                        ToolTip.visible: hovered
                        ToolTip.text: "上移选中节点"
                    }
                    ToolButton {
                        text: "↓"
                        enabled: moveDownAction.enabled
                        onClicked: moveDownAction.trigger()
                        ToolTip.visible: hovered
                        ToolTip.text: "下移选中节点"
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "新节点将添加到  " + (win.sel.name || "根节点") + "  下"
                    color: win.mutedColor
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0d1422"
                    radius: 8
                    border.color: win.borderColor

                    ListView {
                        id: tree
                        anchors.fill: parent
                        anchors.margins: 6
                        clip: true
                        model: scene.treeNodes
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: Rectangle {
                            id: treeRow
                            required property var modelData
                            width: tree.width
                            height: 36
                            radius: 6
                            opacity: win.treeDragging && win.treeDragId === modelData.id ? 0.35 : 1.0
                            color: modelData.id === scene.selectedId
                                   ? "#283452" : (rowMouse.containsMouse ? "#182238" : "transparent")
                            border.color: modelData.id === scene.selectedId ? "#6476d9" : "transparent"

                            Rectangle {
                                visible: modelData.depth > 0
                                x: 12 + (modelData.depth - 1) * 17
                                width: 1
                                height: parent.height
                                color: "#2b3850"
                            }

                            // Indicator: insert before
                            Rectangle {
                                visible: win.treeDragging && win.treeDropTargetId === modelData.id
                                         && win.treeDropMode === 1
                                anchors.left: parent.left
                                anchors.right: parent.right
                                y: -2
                                height: 4
                                radius: 2
                                color: "#ff8794"
                                z: 10
                            }
                            // Indicator: insert after
                            Rectangle {
                                visible: win.treeDragging && win.treeDropTargetId === modelData.id
                                         && win.treeDropMode === 3
                                anchors.left: parent.left
                                anchors.right: parent.right
                                y: parent.height - 2
                                height: 4
                                radius: 2
                                color: "#ff8794"
                                z: 10
                            }
                            // Indicator: become child
                            Rectangle {
                                visible: win.treeDragging && win.treeDropTargetId === modelData.id
                                         && win.treeDropMode === 2
                                anchors.fill: parent
                                color: "#2d4e78"
                                border.color: "#6db8ff"
                                border.width: 2
                                radius: 6
                                z: 5
                            }

                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 10 + modelData.depth * 17
                                spacing: 8
                                Rectangle {
                                    width: 18; height: 18; radius: 5
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: modelData.type === "Geometry" ? "#596de8"
                                         : modelData.type === "Backdrop" ? "#0891b2"
                                         : modelData.type === "Transform" ? "#d28a2d" : "#556176"
                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.type === "Geometry" ? "G"
                                            : modelData.type === "Backdrop" ? "B"
                                            : modelData.type === "Transform" ? "T" : "·"
                                        color: "#ffffff"
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }
                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: -1
                                    Text {
                                        text: modelData.name || win.typeLabel(modelData.type)
                                        color: modelData.visible ? win.textColor : "#667289"
                                        font.pixelSize: 12
                                    }
                                    Text {
                                        text: win.typeLabel(modelData.type)
                                            + (modelData.occluded ? " · 已遮挡" : "")
                                            + (modelData.culled && !modelData.occluded ? " · 已剔除" : "")
                                            + (!modelData.visible ? " · 已隐藏" : "")
                                        color: win.mutedColor
                                        font.pixelSize: 9
                                    }
                                }
                            }

                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton

                                onPressed: function(mouse) {
                                    scene.selectedId = modelData.id
                                    if (mouse.button === Qt.LeftButton) {
                                        beginUserInteraction()
                                        treePressX = mouse.x
                                        treePressY = mouse.y
                                        treeDragId = modelData.id
                                        treeDragName = modelData.name
                                        treeDragging = false
                                        var p = rowMouse.mapToItem(win.contentItem, mouse.x, mouse.y)
                                        treeGhostX = p.x + 10
                                        treeGhostY = p.y + 10
                                    } else if (mouse.button === Qt.RightButton) {
                                        nodeMenu.popup(treeRow, mouse.x, mouse.y)
                                    }
                                }
                                onPositionChanged: function(mouse) {
                                    if (!(mouse.buttons & Qt.LeftButton))
                                        return
                                    if (!win.treeDragging
                                            && (Math.abs(mouse.x - treePressX) + Math.abs(mouse.y - treePressY) > 4))
                                        win.treeDragging = true

                                    if (win.treeDragging) {
                                        var winPt = rowMouse.mapToItem(win.contentItem, mouse.x, mouse.y)
                                        treeGhostX = winPt.x + 12
                                        treeGhostY = winPt.y + 12
                                        var treePt = rowMouse.mapToItem(tree, mouse.x, mouse.y)
                                        calculateTreeDrop(treePt.x, treePt.y)
                                    }
                                }
                                onReleased: function(mouse) {
                                    if (mouse.button === Qt.LeftButton) {
                                        if (win.treeDragging) {
                                            const dragId = win.treeDragId
                                            const dropParentId = win.treeDropParentId
                                            const dropBeforeId = win.treeDropBeforeId
                                            const hasValidDrop = (dragId !== 0 && win.treeDropTargetId !== 0)
                                            win.resetTreeDrag()
                                            if (hasValidDrop) {
                                                Qt.callLater(function() {
                                                    scene.moveNode(dragId, dropParentId, dropBeforeId)
                                                })
                                            }
                                            win.resetTreeDrag()
                                            endUserInteraction()
                                            if (hasValidDrop) {
                                                Qt.callLater(function() {
                                                    scene.moveNode(dragId, dropParentId, dropBeforeId)
                                                })
                                            }
                                        } else {
                                            endUserInteraction()
                                        }
                                    }
                                }
                                onCanceled: {
                                    win.resetTreeDrag()
                                    endUserInteraction()
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "拖动节点可调整顺序或改变父子关系；右键点击节点打开操作菜单。"
                    wrapMode: Text.Wrap
                    color: win.mutedColor
                    font.pixelSize: 10
                }
            }
        }

        Rectangle {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 480
            color: win.color

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Label {
                            text: "场景预览"
                            color: win.mutedColor
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.2
                        }
                        Label {
                            text: "拖动可见节点即可修改几何位置"
                            color: win.textColor
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                    }
                    Rectangle {
                        implicitWidth: damageStatus.implicitWidth + 18
                        implicitHeight: 28
                        radius: 14
                        color: (scene.damageRects.length || scene.damageRectsB.length) ? "#34202c" : "#172638"
                        border.color: (scene.damageRects.length || scene.damageRectsB.length) ? "#8e3f55" : "#2b526a"
                        Label {
                            id: damageStatus
                            anchors.centerIn: parent
                            text: (scene.damageRects.length || scene.damageRectsB.length)
                                  ? ("损伤  A " + scene.damageRects.length + "  ·  B " + scene.damageRectsB.length)
                                  : "无损伤"
                            color: (scene.damageRects.length || scene.damageRectsB.length) ? "#ffacba" : "#8dc8e8"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: "#090e18"
                    border.color: win.borderColor
                    clip: true

                    Item {
                        id: canvas
                        anchors.fill: parent
                        anchors.margins: 10
                        clip: true

                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.fillStyle = "#0b1220"
                                ctx.fillRect(0, 0, width, height)
                                ctx.strokeStyle = "#172238"
                                ctx.lineWidth = 1
                                const s = 24
                                for (let x = 0.5; x < width; x += s) {
                                    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
                                }
                                for (let y = 0.5; y < height; y += s) {
                                    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                                }
                            }
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()
                        }

                        Repeater {
                            model: scene.visualNodes
                            delegate: Item {
                                id: visualItem


                                width: 0
                                height: 0
                                visible: model.visible

                                transform: Matrix4x4 {
                                    matrix: Qt.matrix4x4(
                                        model.m11, model.m21, 0, model.dx,
                                        model.m12, model.m22, 0, model.dy,
                                        0,                        0,                        1, 0,
                                        model.m13, model.m23, 0, model.m33
                                    )
                                }

                                Rectangle {
                                    id: nodeRect
                                    property color baseColor: model.color
                                    property real fillAlpha: model.fullyOpaque
                                                             ? 1.0
                                                             : (model.isBackdrop ? 0.46 : 0.58)
                                    x: model.localX
                                    y: model.localY
                                    width: model.localWidth
                                    height: model.localHeight
                                    color: "transparent"
                                    radius: model.isBackdrop ? 12 : 7
                                    border.color: model.id === scene.selectedId
                                                  ? "#ffffff" : Qt.alpha(Qt.lighter(baseColor, 1.35), 0.95)
                                    border.width: model.id === scene.selectedId ? 2 : 1
                                    gradient: Gradient {
                                        GradientStop {
                                            position: 0
                                            color: Qt.alpha(Qt.lighter(nodeRect.baseColor, 1.35), nodeRect.fillAlpha)
                                        }
                                        GradientStop {
                                            position: 0.55
                                            color: Qt.alpha(nodeRect.baseColor, nodeRect.fillAlpha)
                                        }
                                        GradientStop {
                                            position: 1
                                            color: Qt.alpha(Qt.darker(nodeRect.baseColor, 1.45), nodeRect.fillAlpha)
                                        }
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        height: Math.min(30, parent.height)
                                        radius: parent.radius
                                        color: "#26000000"
                                    }
                                    Text {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.margins: 8
                                        text: model.name
                                            + (model.occluded ? "  ·  已遮挡" : (model.culled ? "  ·  已剔除" : ""))
                                        color: "#ffffff"
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                        style: Text.Outline
                                        styleColor: "#70000000"
                                    }
                                    MouseArea {
                                        property real lastCanvasX: 0
                                        property real lastCanvasY: 0
                                        property bool isDragging: false
                                        property bool interactionStarted: false

                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onPressed: function(mouse) {
                                            if (mouse.button === Qt.LeftButton) {
                                                scene.activateNode(model.id)
                                                isDragging = true
                                                interactionStarted = false
                                                var p = mapToItem(canvas, mouse.x, mouse.y)
                                                lastCanvasX = p.x
                                                lastCanvasY = p.y
                                            } else {
                                                scene.selectedId = model.id
                                                nodeMenu.popup(nodeRect, mouse.x, mouse.y)
                                            }
                                        }
                                        onPositionChanged: function(mouse) {
                                            if (!isDragging || !(mouse.buttons & Qt.LeftButton))
                                                return
                                            var p = mapToItem(canvas, mouse.x, mouse.y)
                                            var dx = p.x - lastCanvasX
                                            var dy = p.y - lastCanvasY
                                            if (dx === 0 && dy === 0)
                                                return
                                            if (!interactionStarted) {
                                                interactionStarted = true
                                                beginUserInteraction()
                                            }
                                            lastCanvasX = p.x
                                            lastCanvasY = p.y
                                            scene.moveSelectedBy(dx, dy)
                                        }
                                        onReleased: function(mouse) {
                                            if (mouse.button === Qt.LeftButton) {
                                                isDragging = false
                                                if (interactionStarted) {
                                                    interactionStarted = false
                                                    scene.finishSelectedMove()
                                                    endUserInteraction()
                                                }
                                            }
                                        }
                                        onCanceled: {
                                            isDragging = false
                                            if (interactionStarted) {
                                                interactionStarted = false
                                                scene.finishSelectedMove()
                                                endUserInteraction()
                                            }
                                        }
                                    }
                                }
                                Rectangle {
                                    id: clipRect
                                    visible: model.type === "Clip"
                                    x: model.localX
                                    y: model.localY
                                    width: model.localWidth
                                    height: model.localHeight
                                    color: Qt.alpha(model.color, 0.08)
                                    border.color: model.id === scene.selectedId ? "#ffffff" : model.color
                                    border.width: model.id === scene.selectedId ? 2 : 1
                                    radius: 2
                                    Text {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.margins: 6
                                        text: model.name
                                        color: "#d8f9ff"
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        style: Text.Outline
                                        styleColor: "#70000000"
                                    }
                                    MouseArea {
                                        property real lastCanvasX: 0
                                        property real lastCanvasY: 0
                                        property bool isDragging: false
                                        property bool interactionStarted: false
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onPressed: function(mouse) {
                                            if (mouse.button === Qt.LeftButton) {
                                                scene.selectedId = model.id
                                                isDragging = true
                                                interactionStarted = false
                                                var p = mapToItem(canvas, mouse.x, mouse.y)
                                                lastCanvasX = p.x
                                                lastCanvasY = p.y
                                            } else {
                                                scene.selectedId = model.id
                                                nodeMenu.popup(clipRect, mouse.x, mouse.y)
                                            }
                                        }
                                        onPositionChanged: function(mouse) {
                                            if (!isDragging || !(mouse.buttons & Qt.LeftButton))
                                                return
                                            var p = mapToItem(canvas, mouse.x, mouse.y)
                                            var dx = p.x - lastCanvasX
                                            var dy = p.y - lastCanvasY
                                            if (dx === 0 && dy === 0)
                                                return
                                            if (!interactionStarted) {
                                                interactionStarted = true
                                                beginUserInteraction()
                                            }
                                            lastCanvasX = p.x
                                            lastCanvasY = p.y
                                            scene.moveSelectedBy(dx, dy)
                                        }
                                        onReleased: function(mouse) {
                                            if (mouse.button === Qt.LeftButton) {
                                                isDragging = false
                                                if (interactionStarted) {
                                                    interactionStarted = false
                                                    scene.finishSelectedMove()
                                                    endUserInteraction()
                                                }
                                            }
                                        }
                                        onCanceled: {
                                            isDragging = false
                                            if (interactionStarted) {
                                                interactionStarted = false
                                                scene.finishSelectedMove()
                                                endUserInteraction()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            x: scene.viewportA.x; y: scene.viewportA.y
                            width: scene.viewportA.width; height: scene.viewportA.height
                            color: "transparent"; border.color: "#78526079"; border.width: 2
                        }
                        Rectangle {
                            x: scene.viewportB.x; y: scene.viewportB.y
                            width: scene.viewportB.width; height: scene.viewportB.height
                            color: "transparent"; border.color: "#78526079"; border.width: 2
                        }
                        Label {
                            x: scene.viewportA.x + 10; y: scene.viewportA.y + 8
                            text: "输出 A"; color: win.mutedColor; font.pixelSize: 9; font.weight: Font.DemiBold
                        }
                        Label {
                            x: scene.viewportB.x + 10; y: scene.viewportB.y + 8
                            text: "输出 B"; color: win.mutedColor; font.pixelSize: 9; font.weight: Font.DemiBold
                        }

                        DamageOverlay {
                            anchors.fill: parent
                            frames: scene.damageFrames
                            historyDuration: win.damageHistoryDuration
                            refreshRate: win.sceneRefreshRate
                        }

                        Connections {
                            target: scene
                            function onSelectedIdChanged() { Qt.callLater(win.revealSelectedNode) }
                        }
                    }
                }
            }
        }

        Rectangle {
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 270
            color: win.panelColor
            border.color: win.borderColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Label {
                    text: "属性检查器"
                    color: win.mutedColor
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "损伤残留时间"
                        color: win.textColor
                        Layout.fillWidth: true
                    }
                    SpinBox {
                        id: damageDurationEditor
                        Layout.preferredWidth: 112
                        from: 100
                        to: 10000
                        stepSize: 100
                        value: win.damageHistoryDuration
                        editable: true
                        onValueModified: win.damageHistoryDuration = value
                        ToolTip.visible: hovered
                        ToolTip.text: "损伤帧保持可见的时间"
                    }
                    Label {
                        text: "ms"
                        color: win.mutedColor
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "刷新率"
                        color: win.textColor
                        Layout.fillWidth: true
                    }
                    SpinBox {
                        id: refreshRateEditor
                        Layout.preferredWidth: 112
                        from: 1
                        to: 360
                        stepSize: 1
                        value: win.sceneRefreshRate
                        editable: true
                        onValueModified: win.sceneRefreshRate = value
                        ToolTip.visible: hovered
                        ToolTip.text: "拖动提交和损伤重绘的刷新率"
                    }
                    Label {
                        text: "Hz"
                        color: win.mutedColor
                        font.pixelSize: 10
                    }
                    ToolButton {
                        text: "↺"
                        onClicked: win.sceneRefreshRate = win.detectedRefreshRate
                        ToolTip.visible: hovered
                        ToolTip.text: "恢复为当前屏幕刷新率"
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 62
                    radius: 8
                    color: win.raisedColor
                    border.color: win.borderColor
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Rectangle {
                            width: 34; height: 34; radius: 9
                            color: win.sel.type === "Geometry" ? "#596de8"
                                 : win.sel.type === "Backdrop" ? "#0891b2"
                                 : win.sel.type === "Transform" ? "#d28a2d" : "#556176"
                            Label {
                                anchors.centerIn: parent
                                text: win.sel.type ? win.sel.type.charAt(0) : "–"
                                color: "#ffffff"
                                font.bold: true
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                text: win.sel.name || "未选择节点"
                                color: win.textColor
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: win.sel.type ? (win.typeLabel(win.sel.type) + "  ·  ID " + win.sel.id)
                                                   : "在节点树或画布中选择节点"
                                color: win.mutedColor
                                font.pixelSize: 10
                            }
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: parent.width
                        visible: !!win.sel.id

                        Label { text: "常规"; color: win.mutedColor; font.pixelSize: 9; font.bold: true }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "固定名称"; color: win.mutedColor }
                            Label {
                                Layout.fillWidth: true
                                text: win.sel.name || ""
                                color: win.textColor
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "可见"; color: win.textColor; Layout.fillWidth: true }
                            Switch { checked: !!win.sel.visible; onToggled: scene.setVisibleSelected(checked) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: !!win.sel.isGeometry
                            Label { text: "完全不透明"; color: win.textColor; Layout.fillWidth: true }
                            Switch { checked: !!win.sel.fullyOpaque; onToggled: scene.setFullyOpaqueSelected(checked) }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: win.borderColor }
                        Label {
                            visible: !!win.sel.isGeometry || !!win.sel.isClip
                            text: win.sel.isClip ? "裁剪区域" : "几何属性"
                            color: win.mutedColor
                            font.pixelSize: 9
                            font.bold: true
                        }
                        GridLayout {
                            visible: !!win.sel.isGeometry || !!win.sel.isClip
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8
                            Label { text: "X"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 2000
                                value: win.num(win.sel.x); editable: true
                                onValueModified: scene.setRectSelected(value, win.num(win.sel.y), win.num(win.sel.w), win.num(win.sel.h))
                            }
                            Label { text: "Y"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 2000
                                value: win.num(win.sel.y); editable: true
                                onValueModified: scene.setRectSelected(win.num(win.sel.x), value, win.num(win.sel.w), win.num(win.sel.h))
                            }
                            Label { text: "宽度"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: 0; to: 2000
                                value: win.num(win.sel.w); editable: true
                                onValueModified: scene.setRectSelected(win.num(win.sel.x), win.num(win.sel.y), value, win.num(win.sel.h))
                            }
                            Label { text: "高度"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: 0; to: 2000
                                value: win.num(win.sel.h); editable: true
                                onValueModified: scene.setRectSelected(win.num(win.sel.x), win.num(win.sel.y), win.num(win.sel.w), value)
                            }
                        }

                        Label {
                            visible: !!win.sel.isTransform
                            text: "变换属性"
                            color: win.mutedColor
                            font.pixelSize: 9
                            font.bold: true
                        }
                        GridLayout {
                            visible: !!win.sel.isTransform
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8
                            Label { text: "水平位移"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 2000
                                value: win.num(win.sel.tx); editable: true
                                onValueModified: scene.setTranslationSelected(value, win.num(win.sel.ty))
                            }
                            Label { text: "垂直位移"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 2000
                                value: win.num(win.sel.ty); editable: true
                                onValueModified: scene.setTranslationSelected(win.num(win.sel.tx), value)
                            }
                            Label { text: "旋转轴"; color: win.mutedColor }
                            ComboBox {
                                id: rotationAxisCombo
                                Layout.fillWidth: true
                                model: [
                                    { text: "Z 轴 (平面旋转)", axis: 2 },
                                    { text: "X 轴 (水平翻转)", axis: 0 },
                                    { text: "Y 轴 (垂直翻转)", axis: 1 }
                                ]
                                textRole: "text"
                                valueRole: "axis"
                                currentIndex: win.num(win.sel.rotationAxis, 2) === 0 ? 1
                                            : (win.num(win.sel.rotationAxis, 2) === 1 ? 2 : 0)
                                onActivated: scene.setRotationSelected(rotationSpinBox.value, currentValue)
                            }
                            Label { text: "旋转角度"; color: win.mutedColor }
                            SpinBox {
                                id: rotationSpinBox
                                Layout.fillWidth: true
                                from: -360
                                to: 360
                                value: Math.round(win.num(win.sel.rotation))
                                editable: true
                                onValueModified: scene.setRotationSelected(value, rotationAxisCombo.currentValue)
                            }
                            Label { text: "水平缩放"; color: win.mutedColor }
                            TextField {
                                Layout.fillWidth: true
                                text: win.num(win.sel.sx, 1).toFixed(3)
                                validator: DoubleValidator { bottom: 0.01; top: 20; decimals: 3 }
                                onEditingFinished: scene.setScaleSelected(Number(text), win.num(win.sel.sy, 1))
                            }
                            Label { text: "垂直缩放"; color: win.mutedColor }
                            TextField {
                                Layout.fillWidth: true
                                text: win.num(win.sel.sy, 1).toFixed(3)
                                validator: DoubleValidator { bottom: 0.01; top: 20; decimals: 3 }
                                onEditingFinished: scene.setScaleSelected(win.num(win.sel.sx, 1), Number(text))
                            }
                        }
                        Label {
                            visible: !!win.sel.isTransform
                            text: "矩阵"
                            color: win.mutedColor
                            font.pixelSize: 9
                            font.bold: true
                        }
                        GridLayout {
                            visible: !!win.sel.isTransform
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: 8
                            rowSpacing: 4
                            Label { text: "m11"; color: win.mutedColor }
                            Label { text: win.num(win.sel.m11).toFixed(3); color: win.textColor }
                            Label { text: "m12  " + win.num(win.sel.m12).toFixed(3); color: win.textColor }
                            Label { text: "m21"; color: win.mutedColor }
                            Label { text: win.num(win.sel.m21).toFixed(3); color: win.textColor }
                            Label { text: "m22  " + win.num(win.sel.m22).toFixed(3); color: win.textColor }
                            Label { text: "dx"; color: win.mutedColor }
                            Label { text: win.num(win.sel.dx).toFixed(3); color: win.textColor }
                            Label { text: "dy  " + win.num(win.sel.dy).toFixed(3); color: win.textColor }

                        }
                        Label {
                            visible: !!win.sel.isBackdrop
                            text: "背景采样属性"
                            color: win.mutedColor
                            font.pixelSize: 9
                            font.bold: true
                        }
                        GridLayout {
                            visible: !!win.sel.isBackdrop
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8
                            Label { text: "扩张范围"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: 0; to: 80
                                value: win.num(win.sel.expansion); editable: true
                                onValueModified: scene.setExpansionSelected(value)
                            }
                            Label { text: "裁剪扩张"; color: win.mutedColor }
                            Switch {
                                checked: win.sel.clipExpansion !== false
                                onToggled: scene.setClipExpansionSelected(checked)
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: win.borderColor }
                        RowLayout {
                            Layout.fillWidth: true
                            Button { text: "上移"; enabled: moveUpAction.enabled; onClicked: moveUpAction.trigger() }
                            Button { text: "下移"; enabled: moveDownAction.enabled; onClicked: moveDownAction.trigger() }
                            Item { Layout.fillWidth: true }
                            Button { text: "标记损伤"; enabled: dirtyAction.enabled; onClicked: dirtyAction.trigger() }
                        }
                        Button {
                            Layout.fillWidth: true
                            text: "删除选中节点"
                            enabled: deleteAction.enabled
                            onClicked: deleteAction.trigger()
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: treeDragGhost
        parent: win.contentItem
        z: 99999
        visible: win.treeDragging
        x: win.treeGhostX
        y: win.treeGhostY
        width: 230
        height: 38
        radius: 8
        color: "#f01c2a3f"
        border.color: "#7399ff"
        border.width: 1.5

        Row {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8
            Rectangle {
                width: 20
                height: 20
                radius: 5
                anchors.verticalCenter: parent.verticalCenter
                color: "#4e73df"
                Label {
                    anchors.centerIn: parent
                    text: "↕"
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: -1
                Label {
                    text: win.treeDragName || "节点"
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                }
                Label {
                    text: win.treeDropHint || "拖动中..."
                    color: "#8cd3ff"
                    font.pixelSize: 9
                }
            }
        }
    }
}

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Waylib.Server
import DamagePlayground

Item {
    id: root

    Shortcut {
        sequences: [StandardKey.Quit]
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: rootOutputItem.motion = !rootOutputItem.motion
    }

    Shortcut {
        sequence: "Ctrl+R"
        context: Qt.ApplicationShortcut
        onActivated: rootOutputItem.resetScene()
    }

    function stackableOf(item) {
        let current = item
        while (current && current.parent && current.parent.objectName !== "sceneRoot")
            current = current.parent
        return current ? current : item
    }

    function raiseItem(item) {
        const target = stackableOf(item)
        const host = target.parent
        if (!host)
            return
        let maxZ = target.z
        for (let i = 0; i < host.children.length; ++i)
            maxZ = Math.max(maxZ, host.children[i].z)
        target.z = maxZ + 1
    }

    function lowerItem(item) {
        const target = stackableOf(item)
        const host = target.parent
        if (!host)
            return
        let minZ = target.z
        for (let i = 0; i < host.children.length; ++i)
            minZ = Math.min(minZ, host.children[i].z)
        target.z = minZ - 1
    }

    component StackInput : Item {
        required property Item stackItem

        anchors.fill: parent

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onPressedChanged: if (pressed)
                root.raiseItem(stackItem)
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: root.lowerItem(stackItem)
        }

        DragHandler {
            target: stackItem
            onActiveChanged: if (active)
                root.raiseItem(stackItem)
        }
    }

    component Actor : Item {
        id: actor
        property string label
        property color ink: "#222222"
        property color paper: "#f4f4f4"
        property Item stackItem: actor

        width: 112
        height: 88

        Rectangle {
            anchors.fill: parent
            color: actor.paper
            border.color: "#6a6a6a"
            border.width: 1
            radius: 4

            Rectangle {
                width: parent.width / 2
                height: parent.height
                color: actor.ink
                radius: 4
            }

            Rectangle {
                anchors.right: parent.right
                width: parent.width / 2
                height: parent.height
                color: actor.paper
                radius: 4
            }
        }

        Text {
            anchors.centerIn: parent
            text: actor.label
            color: "#111111"
            style: Text.Outline
            styleColor: "#ffffff"
            font.pixelSize: 14
            font.bold: true
        }

        StackInput {
            stackItem: actor.stackItem
        }
    }

    component Cover : Rectangle {
        id: cover
        property string label

        width: 220
        height: 150
        radius: 4
        color: "#3a3a3a"
        border.color: "#1a1a1a"
        border.width: 1

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 8
            text: cover.label
            color: "#f0f0f0"
            font.pixelSize: 13
        }

        StackInput {
            stackItem: cover
        }
    }

    OutputRenderWindow {
        id: renderWindow

        width: outputsContainer.implicitWidth
        height: outputsContainer.implicitHeight

        Row {
            id: outputsContainer
            anchors.fill: parent

            DynamicCreatorComponent {
                id: outputDelegateCreator
                creator: Helper.outputCreator

                OutputItem {
                    id: rootOutputItem
                    required property WaylandOutput waylandOutput
                    readonly property OutputViewport onscreenViewport: outputViewport
                    property bool softwareCursor: false
                    property bool motion: true
                    property bool spinRoot: false

                    output: waylandOutput
                    devicePixelRatio: waylandOutput.scale

                    cursorDelegate: Cursor {
                        required property QtObject outputCursor
                        readonly property point position: parent.mapFromGlobal(cursor.position.x, cursor.position.y)

                        cursor: outputCursor.cursor
                        output: outputCursor.output.output
                        x: position.x - hotSpot.x
                        y: position.y - hotSpot.y
                        visible: valid && outputCursor.visible
                        OutputLayer.enabled: !rootOutputItem.softwareCursor
                        OutputLayer.keepLayer: !rootOutputItem.softwareCursor
                        OutputLayer.flags: OutputLayer.Cursor
                        OutputLayer.cursorHotSpot: hotSpot
                        OutputLayer.outputs: [outputViewport]
                    }

                    OutputViewport {
                        id: outputViewport
                        output: waylandOutput
                        devicePixelRatio: parent.devicePixelRatio
                        anchors.centerIn: parent
                    }

                    Canvas {
                        id: wallpaper
                        anchors.fill: parent
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            const s = 24
                            ctx.fillStyle = "#d9d9d9"
                            ctx.fillRect(0, 0, width, height)
                            ctx.fillStyle = "#cfcfcf"
                            for (let y = 0; y < height; y += s) {
                                for (let x = 0; x < width; x += s) {
                                    if (((x / s) + (y / s)) % 2 === 0)
                                        ctx.fillRect(x, y, s, s)
                                }
                            }
                        }
                    }

                    Item {
                        id: stage
                        anchors.fill: parent
                        anchors.topMargin: toolbar.height
                        anchors.bottomMargin: footer.height

                        // Root TransformNode. Child motion is observed through
                        // this matrix so subtree old∪new AABBs can be checked.
                        Item {
                            id: sceneRoot
                            objectName: "sceneRoot"
                            width: parent.width
                            height: parent.height
                            transformOrigin: Item.Center

                            RotationAnimation on rotation {
                                running: rootOutputItem.spinRoot
                                from: 0
                                to: 360
                                duration: 8000
                                loops: Animation.Infinite
                            }

                            Actor {
                                id: rotator
                                objectName: "rotator"
                                label: "rotate"
                                x: sceneRoot.width * 0.08
                                y: sceneRoot.height * 0.12
                                transformOrigin: Item.Center

                                RotationAnimation on rotation {
                                    running: rootOutputItem.motion
                                    from: 0
                                    to: 360
                                    duration: 4000
                                    loops: Animation.Infinite
                                }
                            }

                            Actor {
                                id: mover
                                objectName: "mover"
                                label: "move"
                                property real path: 0
                                x: sceneRoot.width * (0.08 + 0.44 * path)
                                y: sceneRoot.height * 0.48

                                SequentialAnimation on path {
                                    running: rootOutputItem.motion
                                    loops: Animation.Infinite
                                    NumberAnimation {
                                        from: 0
                                        to: 1
                                        duration: 1800
                                        easing.type: Easing.InOutSine
                                    }
                                    NumberAnimation {
                                        from: 1
                                        to: 0
                                        duration: 1800
                                        easing.type: Easing.InOutSine
                                    }
                                }
                            }

                            Actor {
                                id: scaler
                                objectName: "scaler"
                                label: "scale"
                                x: sceneRoot.width * 0.62
                                y: sceneRoot.height * 0.14
                                transformOrigin: Item.Center

                                SequentialAnimation on scale {
                                    running: rootOutputItem.motion
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 1.55; duration: 1100; easing.type: Easing.InOutCubic }
                                    NumberAnimation { to: 0.55; duration: 1100; easing.type: Easing.InOutCubic }
                                }
                            }

                            Item {
                                id: nested
                                objectName: "nested"
                                x: sceneRoot.width * 0.62
                                y: sceneRoot.height * 0.48
                                width: 150
                                height: 150
                                transformOrigin: Item.Center

                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    border.color: "#555555"
                                    border.width: 1
                                }

                                Actor {
                                    id: nestedChild
                                    objectName: "nestedChild"
                                    label: "child"
                                    width: 84
                                    height: 64
                                    anchors.centerIn: parent
                                    stackItem: nested
                                }

                                RotationAnimation on rotation {
                                    running: rootOutputItem.motion
                                    from: 0
                                    to: 360
                                    duration: 7000
                                    loops: Animation.Infinite
                                }

                                StackInput {
                                    stackItem: nested
                                }
                            }

                            Rectangle {
                                id: field
                                x: 16
                                y: sceneRoot.height - 52
                                width: 168
                                height: 32
                                color: "#f4f4f4"
                                border.color: "#6a6a6a"
                                radius: 2

                                Rectangle {
                                    id: caret
                                    objectName: "caret"
                                    x: 10
                                    y: 6
                                    width: 1
                                    height: 20
                                    color: "#111111"

                                    SequentialAnimation on opacity {
                                        running: rootOutputItem.motion
                                        loops: Animation.Infinite
                                        NumberAnimation { to: 0; duration: 400 }
                                        NumberAnimation { to: 1; duration: 400 }
                                    }
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: 18
                                    text: "caret"
                                    color: "#666666"
                                    font.pixelSize: 12
                                }
                            }

                            Cover {
                                id: opaqueCover
                                objectName: "opaqueCover"
                                label: "opaque cover — drag over motion"
                                x: sceneRoot.width * 0.28
                                y: sceneRoot.height * 0.18
                                color: "#404040"
                                z: 1
                            }

                            Cover {
                                id: alphaCover
                                objectName: "alphaCover"
                                label: "alpha 0.55 — drag over motion"
                                x: sceneRoot.width * 0.36
                                y: sceneRoot.height * 0.42
                                color: "#2a2a2a"
                                opacity: 0.55
                                z: 2
                            }

                            RenderBufferBlitter {
                                id: frostGlass
                                objectName: "frostGlass"
                                x: sceneRoot.width * 0.52
                                y: sceneRoot.height * 0.28
                                width: 240
                                height: 170
                                z: 3

                                MultiEffect {
                                    anchors.fill: parent
                                    source: frostGlass.content
                                    autoPaddingEnabled: false
                                    blurEnabled: true
                                    blur: 1.0
                                    blurMax: 48
                                    saturation: 0.1
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    color: "#33ffffff"
                                    border.color: "#222222"
                                    border.width: 1
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.top: parent.top
                                    anchors.topMargin: 8
                                    text: "glass — drag over motion"
                                    color: "#111111"
                                    style: Text.Outline
                                    styleColor: "#ffffff"
                                    font.pixelSize: 13
                                }

                                StackInput {
                                    stackItem: frostGlass
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: toolbar
                        anchors.top: parent.top
                        width: parent.width
                        height: 80
                        color: "#2e2e2e"
                        z: 20

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 6
                            anchors.bottomMargin: 6
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: "Damage"
                                    color: "#f2f2f2"
                                    font.pixelSize: 16
                                    font.bold: true
                                }

                                ComboBox {
                                    id: modeBox
                                    implicitWidth: 132
                                    model: ["highlight", "log", "rerender", "none"]
                                    Component.onCompleted: currentIndex = Math.max(0, model.indexOf(Helper.damageDebugMode))
                                    onActivated: Helper.damageDebugMode = currentText
                                    Connections {
                                        target: Helper
                                        function onDamageDebugModeChanged() {
                                            modeBox.currentIndex = Math.max(0, modeBox.model.indexOf(Helper.damageDebugMode))
                                        }
                                    }
                                }

                                CheckBox {
                                    text: "Motion"
                                    checked: rootOutputItem.motion
                                    onToggled: rootOutputItem.motion = checked
                                }

                                CheckBox {
                                    text: "Software cursor"
                                    checked: rootOutputItem.softwareCursor
                                    onToggled: rootOutputItem.softwareCursor = checked
                                }

                                Item { Layout.fillWidth: true }

                                Button {
                                    text: "Reset"
                                    onClicked: rootOutputItem.resetScene()
                                }

                                Button {
                                    text: "Quit"
                                    onClicked: Qt.quit()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: "Root"
                                    color: "#f2f2f2"
                                    font.pixelSize: 12
                                }

                                Text {
                                    text: "rot"
                                    color: "#c8c8c8"
                                    font.pixelSize: 12
                                }

                                Slider {
                                    id: rootRot
                                    Layout.preferredWidth: 160
                                    from: 0
                                    to: 360
                                    value: sceneRoot.rotation
                                    enabled: !rootOutputItem.spinRoot
                                    onMoved: sceneRoot.rotation = value
                                }

                                Text {
                                    text: Math.round(sceneRoot.rotation) + "°"
                                    color: "#c8c8c8"
                                    font.pixelSize: 12
                                    Layout.preferredWidth: 36
                                }

                                Text {
                                    text: "scale"
                                    color: "#c8c8c8"
                                    font.pixelSize: 12
                                }

                                Slider {
                                    id: rootScale
                                    Layout.preferredWidth: 140
                                    from: 0.4
                                    to: 1.8
                                    value: sceneRoot.scale
                                    onMoved: sceneRoot.scale = value
                                }

                                Text {
                                    text: sceneRoot.scale.toFixed(2)
                                    color: "#c8c8c8"
                                    font.pixelSize: 12
                                    Layout.preferredWidth: 36
                                }

                                CheckBox {
                                    text: "Spin root"
                                    checked: rootOutputItem.spinRoot
                                    onToggled: rootOutputItem.spinRoot = checked
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: footer
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 28
                        color: "#2e2e2e"
                        z: 20

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            color: "#d0d0d0"
                            font.pixelSize: 12
                            text: "Root rot/scale is the parent matrix. Click raise, right-click lower. Drag covers over motion. Space pauses."
                        }
                    }

                    Rectangle {
                        id: sentinel
                        objectName: "sentinel"
                        anchors.top: toolbar.bottom
                        anchors.right: parent.right
                        anchors.margins: 10
                        width: 86
                        height: 28
                        radius: 2
                        color: "#6e6e6e"
                        z: 20

                        Text {
                            anchors.centerIn: parent
                            text: "sentinel"
                            color: "#f2f2f2"
                            font.pixelSize: 12
                        }
                    }

                    function resetScene() {
                        rootOutputItem.spinRoot = false
                        sceneRoot.rotation = 0
                        sceneRoot.scale = 1
                        rotator.x = sceneRoot.width * 0.08
                        rotator.y = sceneRoot.height * 0.12
                        rotator.rotation = 0
                        mover.path = 0
                        mover.y = sceneRoot.height * 0.48
                        scaler.x = sceneRoot.width * 0.62
                        scaler.y = sceneRoot.height * 0.14
                        scaler.scale = 1
                        nested.x = sceneRoot.width * 0.62
                        nested.y = sceneRoot.height * 0.48
                        nested.rotation = 0
                        opaqueCover.x = sceneRoot.width * 0.28
                        opaqueCover.y = sceneRoot.height * 0.18
                        alphaCover.x = sceneRoot.width * 0.36
                        alphaCover.y = sceneRoot.height * 0.42
                        frostGlass.x = sceneRoot.width * 0.52
                        frostGlass.y = sceneRoot.height * 0.28
                    }
                }
            }
        }
    }
}

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import QtQuick.Controls
import Waylib.Server

Item {
    id: root
    anchors.fill: parent

    readonly property int shadowPad: 20
    readonly property int blurPad: 16
    property alias target: target
    property alias sentinel: sentinel
    property alias blurPanel: blurPanel
    property alias frostGlass: frostGlass
    property alias frostBehind: frostBehind
    property alias lockScene: lockScene

    // Same photo as examples/test_glass, not a flat color or star grid.
    Image {
        id: wallpaper
        objectName: "wallpaper"
        anchors.fill: parent
        source: "default-glass-background.jpg"
        fillMode: Image.PreserveAspectCrop
        asynchronous: false
        cache: true
    }

    // Matches waylib/examples/blur: copy the already-drawn output (photo +
    // frostBehind) and MultiEffect-blur that texture. Off by default so
    // dump-vs-grabToImage tests are not comparing a compositor blit.
    Image {
        id: frostBehind
        objectName: "frostBehind"
        visible: frostGlass.visible
        x: 520
        y: 240
        width: 80
        height: 80
        source: "default-glass-background.jpg"
        fillMode: Image.PreserveAspectCrop
        asynchronous: false
    }

    RenderBufferBlitter {
        id: frostGlass
        objectName: "frostGlass"
        visible: false
        x: 500
        y: 220
        width: 200
        height: 160

        MultiEffect {
            anchors.fill: parent
            source: frostGlass.content
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: 64
            saturation: 0.2
        }
    }

    Item {
        id: blurPanel
        objectName: "blurPanel"
        x: 430
        y: 70
        width: 170
        height: 110

        Image {
            id: blurSource
            anchors.fill: parent
            visible: false
            source: "default-glass-background.jpg"
            fillMode: Image.PreserveAspectCrop
            asynchronous: false
        }

        MultiEffect {
            anchors.fill: parent
            source: blurSource
            autoPaddingEnabled: true
            blurEnabled: true
            blur: 0.7
            blurMax: 16
        }

        Rectangle {
            anchors.fill: parent
            color: "#44ffffff"
            border.color: "#88999999"
            radius: 8
        }
    }

    Rectangle {
        id: sentinel
        objectName: "sentinel"
        x: 700
        y: 20
        width: 80
        height: 40
        color: "#3366aa"
        radius: 4
    }

    Item {
        id: target
        objectName: "target"
        x: 80
        y: 80
        width: 80
        height: 80

        Rectangle {
            id: card
            anchors.fill: parent
            radius: 10
            color: "#cc3333"
            visible: false
            layer.enabled: true
        }

        MultiEffect {
            anchors.fill: parent
            source: card
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowBlur: 0.8
            shadowColor: "#99000000"
            shadowVerticalOffset: 6
            blurMax: 20
        }

        Rectangle {
            anchors.fill: parent
            radius: 10
            color: "#cc3333"
            border.color: "#882222"
            border.width: 1
        }
    }

    Popup {
        id: testPopup
        objectName: "testPopup"
        modal: false
        dim: false
        padding: 0
        x: 220
        y: 250
        width: 160
        height: 80

        Rectangle {
            objectName: "popupPanel"
            implicitWidth: 160
            implicitHeight: 80
            color: "#227744"
            border.color: "#115522"
        }
    }

    Item {
        id: nodeCases
        objectName: "nodeCases"
        property var spawned: null

        Rectangle {
            id: visRect
            objectName: "visRect"
            x: 40
            y: 400
            width: 36
            height: 28
            color: "#228844"
        }

        Rectangle {
            id: zBack
            objectName: "zBack"
            x: 100
            y: 400
            width: 36
            height: 28
            z: 0
            color: "#884422"
        }

        Rectangle {
            id: zFront
            objectName: "zFront"
            x: 108
            y: 406
            width: 36
            height: 28
            z: 1
            color: "#224488"
        }

        Rectangle {
            id: opacityRect
            objectName: "opacityRect"
            x: 180
            y: 400
            width: 36
            height: 28
            color: "#aa44aa"
        }

        Rectangle {
            id: ghostRect
            objectName: "ghostRect"
            x: 260
            y: 400
            width: 36
            height: 28
            color: "#44aaaa"
        }

        Rectangle {
            id: occludedBack
            objectName: "occludedBack"
            x: 48
            y: 332
            width: 40
            height: 24
            z: 0
            color: "#336633"
        }

        Rectangle {
            id: opaqueCover
            objectName: "opaqueCover"
            x: 40
            y: 324
            width: 56
            height: 40
            z: 1
            color: "#111111"
        }


        ShaderEffect {
            id: shaderNode
            objectName: "shaderNode"
            x: 340
            y: 400
            width: 48
            height: 32
            property color tint: "#2266cc"
            fragmentShader: "qrc:/wsgdamage/damage_tint.frag.qsb"
        }

        Item {
            id: matrixNode
            objectName: "matrixNode"
            x: 430
            y: 400
            width: 36
            height: 28
            property real tx: 0
            property real ty: 0

            transform: Matrix4x4 {
                matrix: Qt.matrix4x4(
                    1, 0, 0, matrixNode.tx,
                    0, 1, 0, matrixNode.ty,
                    0, 0, 1, 0,
                    0, 0, 0, 1)
            }

            Rectangle {
                anchors.fill: parent
                color: "#cc8822"
            }
        }

        function spawnNode() {
            if (spawned)
                return spawned
            spawned = Qt.createQmlObject(
                'import QtQuick; Rectangle { objectName: "spawnedNode"; x: 520; y: 400; width: 36; height: 28; color: "#33cc66" }',
                nodeCases)
            return spawned
        }

        function despawnNode() {
            if (!spawned)
                return
            spawned.destroy()
            spawned = null
        }

        function resetNodes() {
            despawnNode()
            visRect.visible = true
            visRect.x = 40
            visRect.y = 400
            zBack.z = 0
            zBack.x = 100
            zBack.y = 400
            zFront.z = 1
            opacityRect.opacity = 1
            ghostRect.opacity = 1
            ghostRect.x = 260
            ghostRect.y = 400
            shaderNode.tint = "#2266cc"
            shaderNode.x = 340
            shaderNode.y = 400
            matrixNode.tx = 0
            matrixNode.ty = 0
            occludedBack.color = "#336633"
        }
    }

    function spawnNode() { return nodeCases.spawnNode() }
    function despawnNode() { nodeCases.despawnNode() }
    function resetNodes() { nodeCases.resetNodes() }

    LockScene {
        id: lockScene
        objectName: "lockScene"
        visible: false
    }

    function openPopup() { testPopup.open() }
    function closePopup() { testPopup.close() }

    function showLockscreen() {
        target.visible = false
        blurPanel.visible = false
        frostGlass.visible = false
        lockScene.visible = true
        lockScene.testMode = true
        lockScene.resetLock()
        lockScene.passwordField.forceActiveFocus()
    }

    function openSessionPopup() { lockScene.openSessionPopup() }
    function closeSessionPopup() { lockScene.closeSessionPopup() }
    function scrollSessionList(dy) { lockScene.scrollSessionList(dy) }
    function blinkLockCaret() { lockScene.blinkCaret() }
    function startLockHideSource() { lockScene.startHideSource() }
    function stopLockHideSource() { lockScene.stopHideSource() }

    function resetScene() {
        resetNodes()
        target.x = 80
        target.y = 80
        target.rotation = 0
        target.scale = 1
        target.visible = true
        blurPanel.x = 430
        blurPanel.y = 70
        blurPanel.visible = true
        frostBehind.x = 520
        frostBehind.y = 240
        frostGlass.visible = false
        testPopup.close()
        lockScene.testMode = true
        lockScene.resetLock()
        lockScene.visible = false
    }
}

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import QtQuick.Controls
import Waylib.Server

Item {
    id: root
    anchors.fill: parent

    readonly property int shadowPad: 28
    readonly property int blurPad: 20
    property alias target: target
    property alias sentinel: sentinel
    property alias blurPanel: blurPanel
    property alias frostGlass: frostGlass
    property alias frostBehind: frostBehind

    // Same photo as examples/test_glass, not a flat color or star grid.
    Image {
        id: wallpaper
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

    function openPopup() {
        testPopup.open()
    }

    function closePopup() {
        testPopup.close()
    }

    function resetScene() {
        target.x = 80
        target.y = 80
        target.rotation = 0
        target.scale = 1
        blurPanel.x = 430
        blurPanel.y = 70
        frostBehind.x = 520
        frostBehind.y = 240
        frostGlass.visible = false
        testPopup.close()
    }
}

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

Item {
    id: root

    property alias fragmentShader: shader.fragmentShader
    property bool running: true
    property alias playbackRate: clock.playbackRate
    property alias iTime: clock.time
    property vector2d iResolution: Qt.vector2d(width, height)
    property vector4d iMouse: Qt.vector4d(0, 0, 0, 0)

    function startUp() {
        slowDownAnimation.stop()
        startUpAnimation.stop()
        running = true
        playbackRate = 0.0
        startUpAnimation.start()
    }

    function slowDown(duration) {
        startUpAnimation.stop()
        slowDownAnimation.stop()
        playbackRate = 1.0
        running = true
        slowDownAnimation.duration = duration
        slowDownAnimation.start()
    }

    onRunningChanged: {
        if (running && !slowDownAnimation.running) {
            startUp()
        }
    }

    Component.onCompleted: startUp()

    ShaderFrameClock {
        id: clock

        running: root.running && root.visible
        playbackRate: 0.0
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    ShaderEffect {
        id: shader

        anchors.fill: parent
        property real iTime: root.iTime
        property vector2d iResolution: root.iResolution
        property vector4d iMouse: root.iMouse
    }

    NumberAnimation {
        id: slowDownAnimation

        target: clock
        property: "playbackRate"
        to: 0.0
        easing.type: Easing.OutExpo
        onFinished: root.running = false
    }

    NumberAnimation {
        id: startUpAnimation

        target: clock
        property: "playbackRate"
        to: 1.0
        duration: 650
        easing.type: Easing.OutCubic
    }
}

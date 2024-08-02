// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

Item {
    id: root

    signal stopped

    visible: false
    clip: true

    enum State {
        Show = 1,
        Hide = 2
    }

    property var state: LoginAnimation.Show

    function start(target, pos, to) {
        width = target.width
        height = target.height

        effect.sourceItem = target
        effect.width = target.width
        effect.height = target.height

        xAni.from = pos.x
        xAni.to = to.x

        visible = true
        animation.start()
    }

    function stop() {
        visible = false
        effect.sourceItem = null
        stopped()
    }

    ShaderEffectSource {
        id: effect
        live: true
        hideSource: true
        width: parent.width
        height: parent.height
    }

    Connections {
        target: animation
        function onStopped() {
            stop()
        }
    }

    ParallelAnimation {
        id: animation

        PropertyAnimation {
            id: xAni
            target: effect
            property: "x"
            duration: 800
            easing.type: Easing.OutCubic
        }
        PropertyAnimation {
            target: effect
            property: "opacity"
            duration: 600
            from: root.state === LoginAnimation.Show ? 0 : 1
            to: root.state === LoginAnimation.Show ? 1 : 0
            easing.type: Easing.InQuad
        }
    }
}


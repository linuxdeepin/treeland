// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland

Item {
    id: root

    signal stopped

    visible: false
    clip: true

    enum State {
        Show = 1,
        Hide = 2
    }

    property int state: LoginAnimation.Show
    required property var target

    // Final position of this content, kept so a disabled animation can jump there.
    property point targetPosition: Qt.point(0, 0)
    readonly property bool animationEnabled: WaylandServer.animationConfig.loginContent

    function start(pos, to) {
        targetPosition = to
        if (GreeterProxy.showAnimation && animationEnabled) {
            xAni.from = pos.x
            xAni.to = to.x

            yAni.from = pos.y
            yAni.to = to.y

            effect.sourceItem = root.target

            visible = true
            animation.start()
        } else {
            target.x = to.x
            target.y = to.y
            stop()
        }
    }

    function stop() {
        visible = false
        effect.sourceItem = null
        stopped()
    }

    // Turning the switch off mid-flight finishes this slide once: land on the
    // final position and report completion through the normal stopped signal.
    onAnimationEnabledChanged: {
        if (animationEnabled || !animation.running)
            return
        target.x = targetPosition.x
        target.y = targetPosition.y
        animation.stop()
    }

    ShaderEffectSource {
        id: effect
        live: true
        hideSource: true
        sourceItem: root.target
        width: root.target.width
        height: root.target.height
        x: root.target.x
        y: root.target.y
    }

    Connections {
        target: animation
        function onStopped() {
            stop()
        }
    }

    ParallelAnimation {
        id: animation

        XAnimator {
            id: xAni
            target: effect
            duration: 1000
            easing.type: Easing.OutExpo
        }
        YAnimator {
            id: yAni
            target: effect
            duration: 1000
            easing.type: Easing.OutExpo
        }
        OpacityAnimator {
            target: effect
            duration: 1000
            from: root.state === LoginAnimation.Show ? 0 : 1
            to: root.state === LoginAnimation.Show ? 1 : 0
            easing.type: Easing.OutExpo
        }
    }
}

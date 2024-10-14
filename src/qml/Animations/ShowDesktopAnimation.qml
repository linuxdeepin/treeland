// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Treeland

Item {
    id: root

    signal finished

    clip: false

    required property var target
    required property bool showDesktop
    property int duration: 400 * Helper.animationSpeed

    function start() {
        animation.start();
    }

    ShaderEffectSource {
        id: effect
        live: false
        hideSource: true
        sourceItem: root.target
        x: root.target.x
        y: root.target.y
        width: root.target.width
        height: root.target.height
    }

    ParallelAnimation {
        id: animation
        onFinished: {
            root.finished();
        }
        OpacityAnimator {
            target: effect
            from: showDesktop ? 0 : 1
            to: showDesktop ? 1 : 0
            easing.type: Easing.OutExpo
            duration: root.duration
        }
    }

}

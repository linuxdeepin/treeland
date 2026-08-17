// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
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
    property int duration: 500 * Helper.animationSpeed //500: Initial design requirements

    // The real Decoration (XdgShadow + Border) is a child of `target` laid out at
    // the shadow's bounding rect, which extends beyond the target's
    // [0,0,width,height] by the shadow blur margin. ShaderEffectSource only
    // captures the target's own rect, so the shadow is clipped out of the
    // texture, and `hideSource` simultaneously hides the original shadow. Without
    // a replacement the shadow would pop in/out only after the fade animation
    // finishes instead of following it. Mirror MinimizeAnimation/GeometryAnimation
    // and render a shadow that fades together with the captured content.
    readonly property bool showShadow: target.visibleDecoration && !target.noDecoration

    function start() {
        animation.start();
    }

    Item {
        id: effect
        x: root.target.x
        y: root.target.y
        width: root.target.width
        height: root.target.height

        XdgShadow {
            anchors.fill: parent
            visible: root.showShadow
            cornerRadius: root.target.radius
        }

        ShaderEffectSource {
            anchors.fill: parent
            live: false
            hideSource: true
            sourceItem: root.target
        }
    }

    ParallelAnimation {
        id: animation
        onFinished: {
            // Defer the signal emission to avoid deleting animation objects in the same callback stack.
            Qt.callLater(function() {
                root.finished();
            })
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

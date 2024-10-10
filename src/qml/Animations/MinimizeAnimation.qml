// Copyright (C) 2024 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Treeland

Item {
    id: root

    enum Direction {
        Show = 1, // UnMinimize
        Hide      // Minimize
    }

    signal finished

    clip: false

    required property var target
    required property rect position
    required property var direction
    property int duration: 400 * Helper.animationSpeed

    function start() {
        mainAnimation.start();
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
        id: mainAnimation
        onFinished: {
            root.finished();
        }
        XAnimator {
            target: effect
            from: root.direction === MinimizeAnimation.Direction.Hide ? root.target.x : position.x
            to: root.direction === MinimizeAnimation.Direction.Hide ? position.x : root.target.x
            easing.type: root.direction === MinimizeAnimation.Direction.Hide ? Easing.OutExpo : Easing.InExpo
            duration: root.duration
        }
        YAnimator {
            target: effect
            from: root.direction === MinimizeAnimation.Direction.Hide ? root.target.y : position.y
            to: root.direction === MinimizeAnimation.Direction.Hide ? position.y : root.target.y
            easing.type: root.direction === MinimizeAnimation.Direction.Hide ? Easing.OutExpo : Easing.InExpo
            duration: root.duration
        }
        NumberAnimation {
            target: effect
            property: "width"
            from: root.direction === MinimizeAnimation.Direction.Hide ? root.target.width : position.width
            to: root.direction === MinimizeAnimation.Direction.Hide ? position.width : root.target.width
            easing.type: root.direction === MinimizeAnimation.Direction.Hide ? Easing.OutExpo : Easing.InExpo
            duration: root.duration
        }
        NumberAnimation {
            target: effect
            property: "height"
            from: root.direction === MinimizeAnimation.Direction.Hide ? root.target.height : position.height
            to: root.direction === MinimizeAnimation.Direction.Hide ? position.height : root.target.height
            easing.type: root.direction === MinimizeAnimation.Direction.Hide ? Easing.OutExpo : Easing.InExpo
            duration: root.duration
        }
        OpacityAnimator {
            target: effect
            from: root.direction === MinimizeAnimation.Direction.Hide ? 1 : 0
            to: root.direction === MinimizeAnimation.Direction.Hide ? 0 : 1
            easing.type: root.direction === MinimizeAnimation.Direction.Hide ? Easing.OutExpo : Easing.InExpo
            duration: root.duration
        }
        RotationAnimator {
            target: effect;
            from: root.direction === MinimizeAnimation.Direction.Hide ? 0 : -30;
            to: root.direction === MinimizeAnimation.Direction.Hide ? -30 : 0;
            direction: root.direction === MinimizeAnimation.Direction.Hide ? RotationAnimator.Counterclockwise : RotationAnimator.Clockwise
            duration: root.duration
        }
    }

}

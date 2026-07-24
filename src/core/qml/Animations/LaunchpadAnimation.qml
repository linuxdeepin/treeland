// Copyright (C) 2024-2026 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import Treeland

Item {
    id: root

    enum Direction {
        Show = 1,
        Hide
    }

    signal finished

    visible: false
    clip: true

    required property Item target
    required property int direction
    property int duration: 400 * Helper.animationSpeed

    width: target.width
    height: target.height
    state: "None"

    function start(): void {
        visible = true;
        root.state = direction === LaunchpadAnimation.Direction.Show ? "Show" : "Hide"
    }

    function stop(): void {
        visible = false;
        effect.sourceItem = null;
        finished();
    }

    states: [
        State {
            name: "None"
        },
        State {
            name: "Show"
            PropertyChanges {
                target: effect
                opacity: 1
                scale: 1
            }
        },
        State {
            name: "Hide"
            PropertyChanges {
                target: effect
                opacity: 0
                scale: 0.3
            }
        }
    ]

    transitions: [
        Transition {
            PropertyAnimation {
                target: effect
                property: "opacity"
                duration: root.duration
                easing.type: Easing.OutExpo
            }
            PropertyAnimation {
                target: effect
                property: "scale"
                duration: root.duration
                easing.type: Easing.OutExpo
            }
            onRunningChanged: {
                if (!running) {
                    root.stop()
                }
            }
        }
    ]

    ShaderEffectSource {
        id: effect
        live: root.direction === LaunchpadAnimation.Direction.Show
        hideSource: true
        sourceItem: root.target
        width: root.target.width
        height: root.target.height
        scale: root.direction === LaunchpadAnimation.Direction.Show ? 0.3 : 1
        opacity: root.direction === LaunchpadAnimation.Direction.Show ? 0 : 1
    }
}

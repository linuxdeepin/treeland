// Copyright (C) 2024 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import QtQuick.Effects
import TreeLand.Protocols

Item {
    id: root

    enum Direction {
        Show = 1,
        Hide
    }

    signal stopped

    visible: false
    clip: true

    required property var target
    required property var direction
    required property var position
    property var duration: 400

    width: target.width
    height: target.height
    state: {
        return direction !== LaunchpadAnimation.Direction.Show ? "Show" : "Hide"
    }

    function start() {
        visible = true;
        root.state = direction === LaunchpadAnimation.Direction.Show ? "Show" : "Hide"
    }

    function stop() {
        visible = false;
        effect.sourceItem = null;
        stopped();
    }

    states: [
        State {
            name: "None"
        },
        State {
            name: "Show"
            PropertyChanges {
                target: cover
                opacity: 0.6
            }
            PropertyChanges {
                target: blur
                opacity: 1
            }
            PropertyChanges {
                target: effect
                opacity: 1
                scale: 1
            }
        },
        State {
            name: "Hide"
            PropertyChanges {
                target: cover
                opacity: 0.0
            }
            PropertyChanges {
                target: blur
                opacity: 0
            }
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
                target: cover
                property: "opacity"
                duration: root.duration
                easing.type: Easing.OutExpo
            }
            PropertyAnimation {
                target: blur
                property: "opacity"
                duration: root.duration
                easing.type: Easing.OutExpo
            }
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

    Rectangle {
        id: cover
        anchors.fill: parent
        color: 'black'
        opacity: 0.0
    }

    RenderBufferBlitter {
        id: blitter
        anchors.fill: parent
        MultiEffect {
            id: blur
            anchors.fill: parent
            source: blitter.content
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: 64
            saturation: 0.2
        }
    }

    ShaderEffectSource {
        id: effect
        live: root.direction === LaunchpadAnimation.Direction.Show
        hideSource: true
        sourceItem: root.target
        width: root.target.width
        height: root.target.height
        scale: 0.3
        opacity: 0
    }
}

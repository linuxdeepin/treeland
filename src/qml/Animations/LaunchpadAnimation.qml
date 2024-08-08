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

    function start() {
        visible = true;
        fullscreenAnimation.start();
    }

    function stop() {
        visible = false;
        effect.sourceItem = null;
        stopped();
    }

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
        live: root.direction === LayerShellAnimation.Direction.Show
        hideSource: true
        sourceItem: root.target
        width: root.target.width
        height: root.target.height
    }

    ParallelAnimation {
        id: fullscreenAnimation

        onStopped: {
            root.stop();
        }

        PropertyAnimation {
            target: effect
            property: "opacity"
            duration: root.duration
            from: root.direction === LayerShellAnimation.Direction.Show ? 0 : 1
            to: root.direction !== LayerShellAnimation.Direction.Show ? 0 : 1
            easing.type: Easing.OutExpo
        }
        PropertyAnimation {
            target: blur
            property: "opacity"
            duration: root.duration
            from: root.direction === LayerShellAnimation.Direction.Show ? 0 : 1
            to: root.direction !== LayerShellAnimation.Direction.Show ? 0 : 1
            easing.type: Easing.OutExpo
        }
        PropertyAnimation {
            target: effect
            property: "scale"
            duration: root.duration
            from: root.direction === LayerShellAnimation.Direction.Show ? 0.3 : 1
            to: root.direction !== LayerShellAnimation.Direction.Show ? 0.3 : 1
            easing.type: Easing.OutExpo
        }
        PropertyAnimation {
            target: cover
            property: "opacity"
            duration: root.duration
            from: root.direction === LayerShellAnimation.Direction.Show ? 0.0 : 0.6
            to: root.direction !== LayerShellAnimation.Direction.Show ? 0.0 : 0.6
            easing.type: Easing.OutExpo
        }
    }
}

// Copyright (C) 2024 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

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

    width: target.width
    height: target.height

    function start() {
        visible = true;
        if (root.position === WaylandLayerSurface.AnchorType.None) {
            fullscreenAnimation.start();
        } else {
            sideAnimation.start();
        }
    }

    function stop() {
        visible = false;
        effect.sourceItem = null;
        stopped();
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
        id: sideAnimation
        function onStopped() {
            stop();
        }
        PropertyAnimation {
            target: effect
            property: "x"
            duration: 1000
            from: {
                if (root.position === WaylandLayerSurface.AnchorType.Top || root.position === WaylandLayerSurface.AnchorType.Bottom) {
                    return 0;
                }
                if (root.position === WaylandLayerSurface.AnchorType.Left) {
                    return root.direction === LayerShellAnimation.Direction.Show ? -effect.width : 0;
                }
                if (root.position === WaylandLayerSurface.AnchorType.Right) {
                    return root.direction === LayerShellAnimation.Direction.Show ? effect.width : 0;
                }
            }
            to: {
                if (root.position === WaylandLayerSurface.AnchorType.Top || root.position === WaylandLayerSurface.AnchorType.Bottom) {
                    return 0;
                }
                if (root.position === WaylandLayerSurface.AnchorType.Left) {
                    return root.direction !== LayerShellAnimation.Direction.Show ? -effect.width : 0;
                }
                if (root.position === WaylandLayerSurface.AnchorType.Right) {
                    return root.direction !== LayerShellAnimation.Direction.Show ? effect.width : 0;
                }
            }
            easing.type: Easing.OutExpo
        }
        PropertyAnimation {
            target: effect
            property: "y"
            duration: 1000
            from: {
                if (root.position === WaylandLayerSurface.AnchorType.Left || root.position === WaylandLayerSurface.AnchorType.Right) {
                    return 0;
                }
                if (root.position === WaylandLayerSurface.AnchorType.Top) {
                    return root.direction === LayerShellAnimation.Direction.Show ? -effect.height : 0;
                }
                if (root.position === WaylandLayerSurface.AnchorType.Bottom) {
                    return root.direction === LayerShellAnimation.Direction.Show ? effect.height : 0;
                }
            }
            to: {
                if (root.position === WaylandLayerSurface.AnchorType.Left || root.position === WaylandLayerSurface.AnchorType.Right) {
                    return 0;
                }
                if (root.position === WaylandLayerSurface.AnchorType.Top) {
                    return root.direction !== LayerShellAnimation.Direction.Show ? -effect.height : 0;
                }
                if (root.position === WaylandLayerSurface.AnchorType.Bottom) {
                    return root.direction !== LayerShellAnimation.Direction.Show ? effect.height : 0;
                }
            }
            easing.type: Easing.OutExpo
        }
        PropertyAnimation {
            target: effect
            property: "opacity"
            duration: 1000
            from: root.direction == LayerShellAnimation.Direction.Show ? 0.2 : 1
            to: root.direction == LayerShellAnimation.Direction.Show ? 1 : 0.2
            easing.type: Easing.OutExpo
        }
    }

    ParallelAnimation {
        id: fullscreenAnimation
        function onStopped() {
            stop();
        }
        PropertyAnimation {
            target: effect
            property: "opacity"
            duration: 400
            from: root.direction == LayerShellAnimation.Direction.Show ? 0 : 1
            to: root.direction == LayerShellAnimation.Direction.Show ? 1 : 0
            easing.type: Easing.OutExpo
        }
        PropertyAnimation {
            target: effect
            property: "scale"
            duration: 400
            from: root.direction === LayerShellAnimation.Direction.Show ? 1.4 : 1
            to: root.direction !== LayerShellAnimation.Direction.Show ? 1.4 : 1
            easing.type: Easing.OutExpo
        }
    }
}

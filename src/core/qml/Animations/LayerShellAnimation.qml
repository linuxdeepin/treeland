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

    signal finished

    visible: false
    clip: true

    required property var target
    required property var direction

    property var position: WaylandLayerSurface.AnchorType.Bottom
    property var enableBlur: false

    readonly property rect sourceRect: target.boundingRect

    x: target.x + sourceRect.x
    y: target.y + sourceRect.y
    width: sourceRect.width
    height: sourceRect.height

    function start() {
        visible = true;
        sideAnimation.start();
    }

    function stop() {
        visible = false;
        effect.sourceItem = null;
        finished();
    }

    Loader {
        active: root.enableBlur
        x: effect.x - root.sourceRect.x
        y: effect.y - root.sourceRect.y
        width: root.target.width
        height: root.target.height
        sourceComponent: Blur {
            anchors.fill: parent
            radius: root.target.radius
        }
    }

    ShaderEffectSource {
        id: effect
        live: root.direction === LayerShellAnimation.Direction.Show
        hideSource: true
        sourceItem: root.target
        sourceRect: root.sourceRect
        width: root.width
        height: root.height
    }

    ParallelAnimation {
        id: sideAnimation

        onStopped: {
            root.stop();
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
            from: root.direction === LayerShellAnimation.Direction.Show ? 0.2 : 1
            to: root.direction !== LayerShellAnimation.Direction.Show ? 0.2 : 1
            easing.type: Easing.OutExpo
        }
    }
}

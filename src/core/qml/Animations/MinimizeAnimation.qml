// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
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
    property int animationDuration: duration
    readonly property real minimizedRotation: -30
    readonly property bool showShadow: !target.noDecoration
            && (direction === MinimizeAnimation.Direction.Hide
                ? target.previousSurfaceState === SurfaceWrapper.State.Normal
                : target.surfaceState === SurfaceWrapper.State.Normal)

    function start() {
        configureAnimation(direction === MinimizeAnimation.Direction.Hide
                           ? Qt.rect(target.x, target.y, target.width, target.height)
                           : position,
                           direction === MinimizeAnimation.Direction.Hide
                           ? position
                           : Qt.rect(target.x, target.y, target.width, target.height),
                           direction === MinimizeAnimation.Direction.Hide ? 1 : 0,
                           direction === MinimizeAnimation.Direction.Hide ? 0 : 1,
                           direction === MinimizeAnimation.Direction.Hide ? 0 : minimizedRotation,
                           direction === MinimizeAnimation.Direction.Hide ? minimizedRotation : 0,
                           duration);
        mainAnimation.start();
    }

    function redirect() {
        const currentGeometry = Qt.rect(effect.x, effect.y, effect.width, effect.height);
        const currentOpacity = effect.opacity;
        const currentRotation = effect.rotation;
        mainAnimation.stop();

        const targetGeometry = direction === MinimizeAnimation.Direction.Hide
                ? position
                : Qt.rect(target.x, target.y, target.width, target.height);
        const targetOpacity = direction === MinimizeAnimation.Direction.Hide ? 0 : 1;
        const targetRotation = direction === MinimizeAnimation.Direction.Hide
                ? minimizedRotation
                : 0;
        const fullGeometry = Qt.rect(target.x, target.y, target.width, target.height);
        const remaining = Math.max(
            normalizedDistance(currentGeometry.x, targetGeometry.x,
                               fullGeometry.x, position.x),
            normalizedDistance(currentGeometry.y, targetGeometry.y,
                               fullGeometry.y, position.y),
            normalizedDistance(currentGeometry.width, targetGeometry.width,
                               fullGeometry.width, position.width),
            normalizedDistance(currentGeometry.height, targetGeometry.height,
                               fullGeometry.height, position.height),
            Math.abs(currentOpacity - targetOpacity),
            Math.abs(currentRotation - targetRotation) / Math.abs(minimizedRotation));

        const redirectedDuration = duration > 0
                ? Math.max(1, duration * Math.min(1, remaining))
                : 0;
        configureAnimation(currentGeometry, targetGeometry,
                           currentOpacity, targetOpacity,
                           currentRotation, targetRotation,
                           redirectedDuration);
        mainAnimation.start();
    }

    function normalizedDistance(value, targetValue, firstEndpoint, secondEndpoint) {
        const distance = Math.abs(firstEndpoint - secondEndpoint);
        return distance > 0 ? Math.abs(value - targetValue) / distance : 0;
    }

    function configureAnimation(fromGeometry, toGeometry, fromOpacity, toOpacity,
                                fromRotation, toRotation, newDuration) {
        animationDuration = newDuration;
        xAnimation.from = fromGeometry.x;
        xAnimation.to = toGeometry.x;
        yAnimation.from = fromGeometry.y;
        yAnimation.to = toGeometry.y;
        widthAnimation.from = fromGeometry.width;
        widthAnimation.to = toGeometry.width;
        heightAnimation.from = fromGeometry.height;
        heightAnimation.to = toGeometry.height;
        opacityAnimation.from = fromOpacity;
        opacityAnimation.to = toOpacity;
        rotationAnimation.from = fromRotation;
        rotationAnimation.to = toRotation;
    }

    Item {
        id: effect

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
        id: mainAnimation
        onFinished: {
            // Defer the signal emission to avoid deleting animation objects in the same callback stack.
            Qt.callLater(function() {
                root.finished();
            })
        }
        NumberAnimation {
            id: xAnimation
            target: effect
            property: "x"
            easing.type: Easing.OutExpo
            duration: root.animationDuration
        }
        NumberAnimation {
            id: yAnimation
            target: effect
            property: "y"
            easing.type: Easing.OutExpo
            duration: root.animationDuration
        }
        NumberAnimation {
            id: widthAnimation
            target: effect
            property: "width"
            easing.type: Easing.OutExpo
            duration: root.animationDuration
        }
        NumberAnimation {
            id: heightAnimation
            target: effect
            property: "height"
            easing.type: Easing.OutExpo
            duration: root.animationDuration
        }
        NumberAnimation {
            id: opacityAnimation
            target: effect
            property: "opacity"
            easing.type: Easing.OutExpo
            duration: root.animationDuration
        }
        NumberAnimation {
            id: rotationAnimation
            target: effect
            property: "rotation"
            easing.type: Easing.OutExpo
            duration: root.animationDuration
        }
    }

}

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland

Item {
    id: root

    required property SurfaceWrapper surface
    required property rect fromGeometry
    required property rect toGeometry
    property int duration: 200 * Helper.animationSpeed

    signal ready
    signal finished

    x: fromGeometry.x
    y: fromGeometry.y
    width: fromGeometry.width
    height: fromGeometry.height

    function start() {
        animation.start();
    }

    ShaderEffectSource {
        id: backgroundEffect

        readonly property real xScale: root.width / surface.width
        readonly property real yScale: root.height / surface.height

        live: true
        sourceItem: surface
        hideSource: true
        sourceRect: surface.boundingRect
        width: sourceRect.width * xScale
        height: sourceRect.height * yScale
        x: sourceRect.x * xScale
        y: sourceRect.y * yScale
    }

    ShaderEffectSource {
        id: frontEffect

        readonly property real xScale: root.width / fromGeometry.width
        readonly property real yScale: root.height / fromGeometry.height

        live: false
        sourceItem: surface
        width: sourceRect.width * xScale
        height: sourceRect.height * yScale
        x: sourceRect.x * xScale
        y: sourceRect.y * yScale

        onScheduledUpdateCompleted: {
            root.ready();
        }

        Component.onCompleted: {
            sourceRect = surface.boundingRect
        }
    }

    ParallelAnimation {
        id: animation

        XAnimator {
            target: root
            duration: root.duration
            easing.type: Easing.OutCubic
            from: root.fromGeometry.x
            to: root.toGeometry.x
        }

        YAnimator {
            target: root
            duration: root.duration
            easing.type: Easing.OutCubic
            from: root.fromGeometry.y
            to: root.toGeometry.y
        }

        PropertyAnimation {
            target: root
            property: "width"
            duration: root.duration
            easing.type: Easing.OutCubic
            from: root.fromGeometry.width
            to: root.toGeometry.width
        }

        PropertyAnimation {
            target: root
            property: "height"
            duration: root.duration
            easing.type: Easing.OutCubic
            from: root.fromGeometry.height
            to: root.toGeometry.height
        }

        OpacityAnimator {
            target: frontEffect
            duration: root.duration / 2
            easing.type: Easing.OutCubic
            from: 1.0
            to: 0.0
        }

        onFinished: {
            root.finished();
        }
    }
}

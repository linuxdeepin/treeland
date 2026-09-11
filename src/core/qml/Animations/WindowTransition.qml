// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland
import Waylib.Server
import QtQuick.Effects

Item {
    id: root

    required property SurfaceWrapper surface
    required property rect fromGeometry
    required property rect toGeometry
    property int duration: 400 * Helper.animationSpeed
    property real fadeDurationRatio: 0.5
    property var enableBlur: false
    property var sourceBuffer: null
    property int direction: 0

    signal ready
    signal finished

    x: fromGeometry.x
    y: fromGeometry.y
    width: fromGeometry.width
    height: fromGeometry.height

    function start() {
        animation.start();
    }

    XdgShadow {
        anchors.fill: parent
        visible: surface.visibleDecoration && !surface.noDecoration
        cornerRadius: surface.radius
    }

    Loader {
        active: root.enableBlur
        anchors.fill: parent
        sourceComponent: Blur {
            anchors.fill: parent
            radius: surface.radius
        }
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

    BufferItem {
        id: sourceImage

        anchors.fill: parent
        visible: !!root.sourceBuffer
        buffer: root.sourceBuffer
        smooth: true
        opacity: 0

        Component.onCompleted: {
            opacity = root.direction === 1 ? 1 : 0;
        }
    }

    ParallelAnimation {
        id: animation

        XAnimator {
            target: root
            duration: root.duration
            easing.type: Easing.OutExpo
            from: root.fromGeometry.x
            to: root.toGeometry.x
        }

        YAnimator {
            target: root
            duration: root.duration
            easing.type: Easing.OutExpo
            from: root.fromGeometry.y
            to: root.toGeometry.y
        }

        PropertyAnimation {
            target: root
            property: "width"
            duration: root.duration
            easing.type: Easing.OutExpo
            from: root.fromGeometry.width
            to: root.toGeometry.width
        }

        PropertyAnimation {
            target: root
            property: "height"
            duration: root.duration
            easing.type: Easing.OutExpo
            from: root.fromGeometry.height
            to: root.toGeometry.height
        }

        SequentialAnimation {
            PauseAnimation {
                duration: root.direction === 1 ? 0 : root.duration * (1 - root.fadeDurationRatio)
            }
            PropertyAnimation {
                target: sourceImage
                property: "opacity"
                duration: root.duration * root.fadeDurationRatio
                from: root.direction === 1 ? 1 : 0
                to: root.direction === 1 ? 0 : 1
                easing.type: Easing.Linear
            }
        }

        onFinished: {
            // Defer the signal emission to avoid deleting animation objects in the same callback stack.
            Qt.callLater(function() {
                root.finished();
            })
        }
    }

    Component.onCompleted: {
        root.ready();
    }
}

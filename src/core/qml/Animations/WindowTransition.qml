// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland
import Waylib.Server
import QtQuick.Effects

Item {
    id: root

    enum Direction {
        Show = 1,
        Hide
    }

    required property SurfaceWrapper surface
    required property rect fromGeometry
    required property rect toGeometry
    property int duration: 400 * WaylandServer.animationSpeed
    property var enableBlur: false
    property var sourceSurface: null
    property int direction: 0

    readonly property bool opening: direction === WindowTransition.Direction.Show
    readonly property bool hasIcon: !!sourceSurface

    property real progress: hasIcon && opening ? 0 : 1

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
        visible: surface.visibleDecoration && !surface.noDecoration && surface.shadowVisible
        opacity: root.progress
        cornerRadius: surface.radius
        shadowBlur: surface.shadowBlurRadius
        shadowOffsetX: surface.shadowOffsetX
        shadowOffsetY: surface.shadowOffsetY
        shadowColor: surface.shadowColor
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
        id: windowSource

        // Sampled by the effect below, never drawn directly.
        visible: false
        live: true
        sourceItem: surface
        hideSource: true
        anchors.fill: parent
    }

    SurfaceItemContent {
        id: sourceImage

        width: implicitWidth
        height: implicitHeight
        visible: root.hasIcon
        surface: root.sourceSurface
        smooth: true
        live: true
    }

    ShaderEffectSource {
        id: iconSource

        visible: false
        live: true
        sourceItem: sourceImage
        hideSource: true
        anchors.fill: parent
    }

    ShaderEffect {
        anchors.fill: parent

        property real progress: root.progress
        property variant windowTexture: windowSource
        property variant iconTexture: iconSource
        fragmentShader: "qrc:/shaders/window_transition.frag.qsb"
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

        // The window's visible weight follows OutExpo, the icon's follows
        // 1 - OutExpo.
        NumberAnimation {
            target: root
            property: "progress"
            duration: root.duration
            easing.type: Easing.OutExpo
            from: root.hasIcon && root.opening ? 0 : 1
            to: root.hasIcon && !root.opening ? 0 : 1
        }

        onFinished: {
            // Defer the signal emission to avoid deleting animation objects in the same callback stack.
            Qt.callLater(function() {
                root.finished();
            })
        }
    }
}

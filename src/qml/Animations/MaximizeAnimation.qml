// Copyright (C) 2024 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Waylib.Server
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

Item {
    id: root

    signal stopped

    visible: false
    clip: true

    required property var target
    required property var decoration
    property D.Palette outerShadowColor: DS.Style.highlightPanel.dropShadow
    property int duration: 500
    property real radius: root.decoration.radius
    property var stopHandler
    property bool enableBlur: false

    readonly property var initWidth: {
        return target.width
    }
    readonly property var initHeight: {
        return target.height
    }
    readonly property var initX: {
        return target.x
    }
    readonly property var initY: {
        return target.y
    }

    Behavior on radius {
        NumberAnimation { duration: 500 }
    }

    function setup() {
        cover.sourceItem = target

        cover.x = initX
        cover.y = initY
        cover.width = initWidth
        cover.height = initHeight

        // For reset
        if (root.decoration.radius === 0) {
            root.radius = 15
        }

        visible = true;
    }

    function start() {
        state = "show"
    }

    function stop() {
        visible = false;
        if (root.stopHandler) {
            stopHandler()
        }
        cover.sourceItem = null;
        stopped();
    }

    state: "normal"
    states: [
        State {
            name: "normal"
            PropertyChanges {
                target: cover
                x: initX
                y: initY
                height: initHeight
                width: initWidth
            }
        },
        State {
            name: "show"
            PropertyChanges {
                target: cover
                x: target.x
                y: target.y
                width: target.width
                height: target.height
            }
        }
    ]

    transitions: [
        Transition {
            PropertyAnimation {
                target: cover
                duration: root.duration
                properties: 'x,y,width,height'
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
        id: cover
        live: true
        hideSource: true
        sourceItem: root.target
        anchors.fill: parent
        visible: false
    }

    D.BoxShadow {
        anchors.fill: cover
        shadowColor: root.outerShadowColor
        shadowOffsetY: 4
        shadowBlur: 16
        cornerRadius: root.radius
        hollow: true
    }

    MultiEffect {
        enabled: root.radius > 0
        anchors.fill: cover
        source: cover
        maskEnabled: true
        maskSource: mask
    }

    Item {
        id: mask
        anchors.fill: cover
        layer.enabled: true
        visible: false
        Rectangle {
            anchors.fill: parent
            radius: root.radius
        }
    }
}

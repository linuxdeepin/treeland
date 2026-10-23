// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Treeland
import LockScreen

FocusScope {
    id: root
    clip: true

    signal hideAnimationFinished()

    required property QtObject output
    required property QtObject outputItem
    visible: true

    x: outputItem.x
    y: outputItem.y
    width: outputItem.width
    height: outputItem.height

    palette.windowText: Qt.rgba(1.0, 1.0, 1.0, 1.0)

    /**************/
    /* Components */
    /**************/

    Rectangle {
        id: background

        readonly property int scaleDuration: 1000
        readonly property int duration: (GreeterProxy.isLocked || scale !== 1) ? scaleDuration : 600
        readonly property bool hasWallpaper: wallpaper.surface !== null
        property real effectStrength: 0.0
        anchors.fill: parent
        clip: true
        color: hasWallpaper ? 'transparent' : 'black'
        opacity: 0.0
        transformOrigin: Item.Center
        scale: GreeterProxy.isLocked ? 1.2 : 1
        state: (GreeterProxy.isLocked || GreeterProxy.undecided || GreeterProxy.showShutdownView) ? "Show" : "Hide"
        states: [
            State {
                name: "Show"
                PropertyChanges {
                    target: background
                    effectStrength: 1.0
                    opacity: 1
                }
            },
            State {
                name: "Hide"
                PropertyChanges {
                    target: background
                    effectStrength: 0.0
                    opacity: 0.0
                }
            }
        ]

        Behavior on scale {
            enabled: WaylandServer.config.greeterBackgroundAnimationEnabled
            NumberAnimation {
                duration: background.scaleDuration
                easing.type: Easing.OutExpo
            }
        }

        transitions: [
            Transition {
                from: "*"
                to: "Show"
                enabled: WaylandServer.config.greeterBackgroundAnimationEnabled
                PropertyAnimation {
                    property: "effectStrength"
                    duration: background.duration
                    easing.type: Easing.OutExpo
                }
                PropertyAnimation {
                    property: "opacity"
                    duration: background.duration
                    easing.type: Easing.OutExpo
                }
            },
            Transition {
                from: "*"
                to: "Hide"
                enabled: WaylandServer.config.greeterBackgroundAnimationEnabled
                PropertyAnimation {
                    property: "effectStrength"
                    duration: background.duration
                    easing.type: Easing.OutExpo
                }
                PropertyAnimation {
                    property: "opacity"
                    duration: background.duration
                    easing.type: Easing.OutExpo
                }
            }
        ]
        onStateChanged: {
            if (state === "Show") {
                // Only a real lock screen entry zooms the desktop wallpaper.
                if (GreeterProxy.isLocked || GreeterProxy.undecided)
                    WaylandServer.startLockscreen(root.output, true);
                wallpaper.play = true;
            } else {
                wallpaper.play = false;
                WaylandServer.showDesktop(root.output)
            }
        }
        onOpacityChanged: {
            if (state === "Hide" && opacity === 0.0)
                root.hideAnimationFinished()
        }

        Wallpaper {
            id: wallpaper

            anchors.fill: parent
            clip: true
            wallpaperRole: Wallpaper.Lockscreen
            output: root.output
            visible: false
        }

        ShaderEffect {
            id: wallpaperEffect

            anchors.fill: parent
            clip: true
            visible: background.hasWallpaper

            property variant source: wallpaper
            property real u_dimRatio: 0.25
            property real u_strength: background.effectStrength

            fragmentShader: "qrc:/shaders/wallpaper_luma_dim.frag.qsb"
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: true
    }

}

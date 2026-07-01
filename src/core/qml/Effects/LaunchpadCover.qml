// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Treeland
import Waylib.Server

Item {
    id: root
    clip: true

    required property SurfaceWrapper wrapper
    required property WaylandOutput output

    property bool mapped: false

    z: (wrapper.z ?? 0) - 1
    anchors.fill: wrapper

    Wallpaper {
        id: wallpaper
        output: root.output
        workspace: Helper.workspace.current
    }

    ShaderEffectSource {
        id: wallpaperSource
        sourceItem: wallpaper
        recursive: true
        live: true
        smooth: true
        anchors.fill: parent
        hideSource: false
        state: mapped ? "Scale" : "Normal"
        states: [
            State {
                name: "Normal"
                PropertyChanges {
                    target: wallpaperSource
                    scale: 1
                }
            },
            State {
                name: "Scale"
                PropertyChanges {
                    target: wallpaperSource
                    scale: 1.4
                }
            }
        ]

        transitions: [
            Transition {
                from: "*"
                to: "Normal"
                PropertyAnimation {
                    property: "scale"
                    duration: 1000
                    easing.type: Easing.OutExpo
                }
            },
            Transition {
                from: "*"
                to: "Scale"
                PropertyAnimation {
                    property: "scale"
                    duration: 1000
                    easing.type: Easing.OutExpo
                }
            }
        ]
    }

    Blur {
        anchors.fill: parent
        z: wallpaperSource.z + 1
    }

    Rectangle {
        id: cover
        anchors.fill: parent
        color: 'black'
        opacity: 0.6
    }

    opacity: root.mapped ? 1 : 0
    Behavior on opacity {
        PropertyAnimation {
            duration: 400
            easing.type: Easing.OutExpo
        }
    }

    onMappedChanged: {
        Helper.setLaunchpadMapped(root.output, root.mapped)
    }
}

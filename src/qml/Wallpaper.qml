// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import TreeLand.Utils

WallpaperProxy {
    clip: true
    AnimatedImage {
        id: background
        source: parent.source

        fillMode: Image.PreserveAspectCrop
        anchors.fill: parent
        asynchronous: true

        states: [
            State {
                name: "Normal"
                PropertyChanges {
                    target: background
                    scale: 1
                }
            },
            State {
                name: "Scale"
                PropertyChanges {
                    target: background
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

    onTypeChanged: {
        background.state = type === Helper.Normal ? "Normal" : "Scale";
    }
}

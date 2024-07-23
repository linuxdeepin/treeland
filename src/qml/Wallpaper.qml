// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import TreeLand.Utils

WallpaperController {
    Image {
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
                    scale: 1.2
                }
            }
        ]

        transitions: [
            Transition {
                from: "*"
                to: "Normal"
                PropertyAnimation {
                    property: "scale"
                    duration: 400
                    easing.type: Easing.OutExpo
                }
            },
            Transition {
                from: "*"
                to: "Scale"
                PropertyAnimation {
                    property: "scale"
                    duration: 400
                    easing.type: Easing.OutExpo
                }
            }
        ]
    }

    onAnimationTypeChanged: {
        background.state = animationType === WallpaperController.Normal ? "Normal" : "Scale"
    }
}

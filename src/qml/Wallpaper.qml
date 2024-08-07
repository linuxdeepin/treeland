// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import TreeLand.Utils

WallpaperProxy {
    clip: true
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

    Rectangle {
        id: cover
        anchors.fill: parent
        color: 'black'
        opacity: 0.0
        state: background.state
        states: [
            State {
                name: "Normal"
                PropertyChanges {
                    target: cover
                    opacity: 0.0
                }
            },
            State {
                name: "Scale"
                PropertyChanges {
                    target: cover
                    opacity: 0.6
                }
            }
        ]

        transitions: [
            Transition {
                from: "*"
                to: "Normal"
                PropertyAnimation {
                    property: "opacity"
                    duration: 1000
                    easing.type: Easing.OutExpo
                }
            },
            Transition {
                from: "*"
                to: "Scale"
                PropertyAnimation {
                    property: "opacity"
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
